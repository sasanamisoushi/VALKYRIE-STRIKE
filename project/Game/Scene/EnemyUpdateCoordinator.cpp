#include "EnemyUpdateCoordinator.h"

#include "BossClearSequenceController.h"
#include "BossEncounterState.h"
#include "CombatAlertState.h"
#include "EnemySpawnManager.h"
#include "SongState.h"
#include "TitleBackgroundEnemyState.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/enemy/EnemyEventManager.h"
#include "Game/enemy/GroundEnemy.h"
#include "Game/obstacle/Obstacle.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/math/MyMath.h"

#include <Windows.h>

void EnemyUpdateCoordinator::UpdateEnemies(const EnemyBehaviorUpdateContext& context, Vector3& playerPos) {
	if (!context.enemies || !context.obstacles) {
		return;
	}

	auto& enemies = *context.enemies;
	const auto& obstacles = *context.obstacles;

	std::vector<Vector3> enemyBulletHits;
	if (context.enemyBulletManager) {
		// タイトル背景でも敵弾は必ず更新する。プレイヤーを nullptr にして、
		// 動きは見せつつ、放置デモも決められたタイミングまでダメージを受けないようにする。
		context.enemyBulletManager->Update(
			context.isAutoFlightMode ? nullptr : context.player,
			enemyBulletHits,
			obstacles);
		const uint64_t attackSerial = context.enemyBulletManager->GetAttackSerial();
		if (!context.isAutoFlightMode && context.combatAlertState && attackSerial > observedEnemyAttackSerial_) {
			// 発射された敵弾を検出した時だけ、回避を促すアラートを点灯する。
			context.combatAlertState->TriggerEnemyAttack(context.enemyBulletManager->GetLastAttackPosition());
		}
		// ステージ再読み込みで弾マネージャが初期化され、通番が0へ戻った場合も同期する。
		observedEnemyAttackSerial_ = attackSerial;
	}

	if (context.explosionManager && !enemyBulletHits.empty()) {
		context.explosionManager->CreateHitEffects(enemyBulletHits);
	}

	// ボス本体を固定したまま、通常敵同士の重なりだけを解消する。
	enemySeparationResolver_.Resolve(enemies);

	for (auto& enemy : enemies) {
		// 接触ダメージは発生させず、めり込みだけを即座に解消する。
		if (!context.isAutoFlightMode && context.player && !context.player->IsDead() &&
			enemy->CanReceiveDamage()) {
			if (playerEnemyContactResolver_.Resolve(*context.player, *enemy)) {
				playerPos = context.player->GetPosition();
			}
		}

		// 近接攻撃の当たり判定
		if (context.player && context.player->IsMeleeAttacking()) {
			OBB meleeHitbox = context.player->GetMeleeHitbox();
			Sphere enemySphere;
			enemySphere.center = enemy->GetPosition();
			enemySphere.radius = enemy->GetCollisionRadius();
			if (MyMath::IsCollision(enemySphere, meleeHitbox)) {
				enemy->TakeDamage(context.player->GetMeleeDamage());
			}
		}

		// 近接攻撃の当たり判定（近接攻撃を持つ敵のみ有効）
		if (!context.isAutoFlightMode && enemy->IsMeleeActive() &&
			context.player && !context.player->IsDead()) {
			// 飛び蹴りの有効フレーム中は、弾と同じ危険アラートを維持する。
			if (context.combatAlertState) {
				context.combatAlertState->TriggerEnemyAttack(enemy->GetPosition());
			}
			OBB meleeOBB = enemy->GetMeleeBoxOBB();
			OBB playerOBB = context.player->GetOBB();
			if (MyMath::IsCollision(meleeOBB, playerOBB)) {
				context.player->TakeDamage(1);
			}
		}

		enemy->Update(playerPos, context.enemyBulletManager, obstacles);
		if (GroundEnemy* groundEnemy = dynamic_cast<GroundEnemy*>(enemy.get())) {
			groundEnemyEffectProcessor_.Process(*groundEnemy, context.explosionManager, context.particleManager);
		}
		if (Boss* boss = dynamic_cast<Boss*>(enemy.get())) {
			bossMinionSpawner_.SpawnRequestedMinions(*boss, context.bossMaxActiveMinions, enemies);
		}
	}
}

void EnemyUpdateCoordinator::ProcessDefeatedEnemies(const EnemyDefeatHandlingContext& context) {
	if (!context.enemies) {
		return;
	}

	auto& enemies = *context.enemies;
	for (auto it = enemies.begin(); it != enemies.end(); ) {
		if (!(*it)->IsDead()) {
			++it;
			continue;
		}

		const bool defeatedBoss = (*it)->IsBoss();
		if (defeatedBoss && !context.isAutoFlightMode && !context.isGameOverActive) {
			if (context.bossClearSequence &&
				context.bossClearSequence->TryBegin(it->get(), context.player, context.clearEnemyReferences)) {
				if (context.defeatedEnemyCount) {
					++(*context.defeatedEnemyCount);
				}
			}
			++it;
			continue;
		}
		if (context.isAutoFlightMode) {
			// タイトル背景でも撃破された敵は残さない。倒した後は画面外から再出現させる。
			if (context.clearEnemyReferences) {
				context.clearEnemyReferences(it->get());
			}
			it = enemies.erase(it);
			if (context.titleBackgroundEnemyState) {
				context.titleBackgroundEnemyState->SetRespawnDelay(45); // 撃破後、約0.75秒後に次の敵を画面外から出現
			}
			continue;
		}
		if (context.clearEnemyReferences) {
			context.clearEnemyReferences(it->get());
		}
		if (context.enemySpawns && context.enemyEventManager && context.enemySpawnManager && context.songState) {
			enemyDefeatProcessor_.ProcessStandardDefeat(
				*(*it),
				*context.enemySpawns,
				*context.enemyEventManager,
				*context.enemySpawnManager,
				enemies,
				*context.songState);
		}
		if (context.defeatedEnemyCount) {
			++(*context.defeatedEnemyCount);
		}
		it = enemies.erase(it);
	}
}

bool EnemyUpdateCoordinator::UpdateWaveAndBossProgression(const EnemyWaveProgressionContext& context) {
	if (!context.enemySpawns || !context.enemies || !context.obstacles || !context.enemySpawnManager) {
		return false;
	}

	auto& enemySpawns = *context.enemySpawns;
	auto& enemies = *context.enemies;
	auto& obstacles = *context.obstacles;

	context.enemySpawnManager->Update(enemySpawns, enemies, obstacles);

	bool hasAnyEnemySpawned = false;
	for (const auto& spawn : enemySpawns) {
		if (!spawn.isBoss && spawn.hasSpawned) {
			hasAnyEnemySpawned = true;
			break;
		}
	}

	if (!context.isAutoFlightMode && !context.isGameOverActive &&
		hasAnyEnemySpawned && enemies.empty() && !context.enemySpawnManager->HasPendingSpawns()) {
		if (context.bossEncounterState && !context.bossEncounterState->IsBossSpawned()) {
			OutputDebugStringA("[GamePlayScene] All enemies defeated! Spawning Boss...\n");
			bossSpawnController_.Spawn(context.targetBossPos, context.bossTuning, obstacles, enemies);
			// 警告を挟んでから戦闘へ入る。表示中は入力・敵AI・弾更新を止めているため、プレイヤーは安全に認識できる。
			context.bossEncounterState->BeginIntro();
		} else {
			OutputDebugStringA("[GamePlayScene] Boss defeated! Changing scene to CLEAR.\n");
			return true;
		}
	}

	return false;
}
