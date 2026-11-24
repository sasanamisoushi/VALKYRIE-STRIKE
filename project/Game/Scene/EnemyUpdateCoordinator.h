#pragma once

#include "BossMinionSpawner.h"
#include "BossSpawnController.h"
#include "EnemyDefeatProcessor.h"
#include "EnemySeparationResolver.h"
#include "GroundEnemyEffectProcessor.h"
#include "PlayerEnemyContactResolver.h"
#include "Game/enemy/Boss.h"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <vector>

class BossClearSequenceController;
class BossEncounterState;
class CombatAlertState;
class Enemy;
class EnemyBulletManager;
class EnemyEventManager;
class EnemySpawnManager;
class ExplosionManager;
class Obstacle;
class ParticleManager;
class Player;
class SongState;
class TitleBackgroundEnemyState;
struct EnemySpawnData;

// 敵更新（敵弾・分離・接触・近接・AI更新・エフェクト・増援要求）に必要な文脈。
struct EnemyBehaviorUpdateContext {
	bool isAutoFlightMode = false;
	int bossMaxActiveMinions = 0;
	Player* player = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	EnemyBulletManager* enemyBulletManager = nullptr;
	ExplosionManager* explosionManager = nullptr;
	ParticleManager* particleManager = nullptr;
	CombatAlertState* combatAlertState = nullptr;
};

// 撃破処理（ボス撃破演出開始・タイトル背景再湧き・通常敵撃破イベントと削除）に必要な文脈。
struct EnemyDefeatHandlingContext {
	bool isAutoFlightMode = false;
	bool isGameOverActive = false;
	Player* player = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	EnemyEventManager* enemyEventManager = nullptr;
	EnemySpawnManager* enemySpawnManager = nullptr;
	SongState* songState = nullptr;
	BossClearSequenceController* bossClearSequence = nullptr;
	TitleBackgroundEnemyState* titleBackgroundEnemyState = nullptr;
	int* defeatedEnemyCount = nullptr;
	std::function<void(Enemy*)> clearEnemyReferences;
};

// ウェーブ湧き・全滅時のボス出現およびクリア判定に必要な文脈。
struct EnemyWaveProgressionContext {
	bool isAutoFlightMode = false;
	bool isGameOverActive = false;
	Vector3 targetBossPos{};
	const Boss::Tuning* bossTuning = nullptr;
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	EnemySpawnManager* enemySpawnManager = nullptr;
	BossEncounterState* bossEncounterState = nullptr;
};

// 敵更新・撃破処理・ボス進行の3責務を明確に分けて統括する。
class EnemyUpdateCoordinator {
public:
	// 敵弾・分離・接触・近接・AI更新を実行する。
	void UpdateEnemies(const EnemyBehaviorUpdateContext& context, Vector3& playerPos);

	// 撃破済み敵の演出開始・増援イベント・削除を実行する。
	void ProcessDefeatedEnemies(const EnemyDefeatHandlingContext& context);

	// 敵ウェーブとボス出現判定を更新する。ボス撃破後のフォールバックCLEAR遷移が必要なら true を返す。
	bool UpdateWaveAndBossProgression(const EnemyWaveProgressionContext& context);

private:
	EnemySeparationResolver enemySeparationResolver_;
	PlayerEnemyContactResolver playerEnemyContactResolver_;
	GroundEnemyEffectProcessor groundEnemyEffectProcessor_;
	BossMinionSpawner bossMinionSpawner_;
	BossSpawnController bossSpawnController_;
	EnemyDefeatProcessor enemyDefeatProcessor_;
	uint64_t observedEnemyAttackSerial_ = 0;
};
