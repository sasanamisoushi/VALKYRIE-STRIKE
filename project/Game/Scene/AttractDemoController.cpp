#include "AttractDemoController.h"

#include "BossEncounterState.h"
#include "EnemySpawnManager.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Boss.h"
#include "Game/enemy/Enemy.h"

#include <cmath>

namespace {
Vector3 AddVector3(const Vector3& a, const Vector3& b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 ScaleVector3(const Vector3& v, float scale) {
	return { v.x * scale, v.y * scale, v.z * scale };
}

Vector3 NormalizeOrVector3(const Vector3& v, const Vector3& fallback) {
	const float length = MyMath::Length(v);
	if (length <= 0.0001f) {
		return fallback;
	}
	return { v.x / length, v.y / length, v.z / length };
}
}

void AttractDemoController::UpdateSequence(
	Player* player,
	std::list<std::unique_ptr<Enemy>>& enemies,
	std::vector<EnemySpawnData>& enemySpawns,
	EnemySpawnManager& enemySpawnManager,
	BossEncounterState& bossEncounterState,
	const std::function<void()>& clearAllEnemyReferences) {
	state_.AdvanceFrame();
	if (!state_.IsClearCinematic()) {
		// 通常のデモプレイは従来どおり直進飛行を続け、自然な戦闘状況だけを表示する。
		return;
	}
	if (!state_.IsBossCreated()) {
		// TAB で始めるデモは、ステージの攻略条件に依存せず直ちにクリア演出を見せる。
		// 既存の敵が混ざると二重に撃破演出が始まるため、デモ用のボス1体だけを置く。
		if (clearAllEnemyReferences) {
			clearAllEnemyReferences();
		}
		enemies.clear();
		enemySpawns.clear();
		enemySpawnManager.Clear();
		bossEncounterState.MarkBossSpawned();

		if (!player) {
			return;
		}

		const Vector3 forward = NormalizeOrVector3(player->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
		auto boss = std::make_unique<Boss>();
		const Vector3 bossPosition = AddVector3(
			AddVector3(player->GetPosition(), ScaleVector3(forward, 28.0f)), { 0.0f, 8.0f, 0.0f });
		boss->Initialize(bossPosition);
		boss->SetRotation({ 0.0f, std::atan2(-forward.x, -forward.z), 0.0f });
		boss->UpdateModel();
		enemies.push_back(std::move(boss));
		state_.MarkBossCreated();
	}

	// 最初の一瞬だけボス全体を見せてから、通常のダメージ経路で撃破する。
	// 以降の爆散・MISSION CLEAR・リザルトは通常プレイと同じ処理を使用する。
	if (state_.GetFrame() < AttractDemoState::kClearCinematicLeadFrames) {
		return;
	}
	for (const auto& enemy : enemies) {
		if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
			enemy->TakeDamage(enemy->GetHP());
			return;
		}
	}
}

void AttractDemoController::UpdatePilot(Player* player) const {
	if (!player || state_.IsPilotControlLocked()) {
		return;
	}

	const float speed = player->GetModeParams(PlayerMode::Fighter).maxMoveSpeed * 0.78f;
	const Vector3& dir = state_.GetFlightDirection();
	Vector3 position = player->GetPosition();
	position.x += dir.x * speed;
	position.y += dir.y * speed;
	position.z += dir.z * speed;
	player->SetVelocity({
		dir.x * speed,
		dir.y * speed,
		dir.z * speed,
	});
	player->UpdatePresentation(position, { 0.0f, state_.GetFlightYaw(), 0.0f }, 2.2f, true);
}
