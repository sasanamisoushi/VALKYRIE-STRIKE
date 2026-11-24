#pragma once

#include "AttractDemoState.h"
#include "engine/Utility/StageLoader.h"
#include "engine/math/MyMath.h"

#include <functional>
#include <list>
#include <memory>
#include <vector>

class BossEncounterState;
class Enemy;
class EnemySpawnManager;
class Player;

// アトラクトデモの進行・デモ用ボス生成・自動飛行制御をまとめて管理する。
class AttractDemoController {
public:
	void Reset(bool clearCinematic) { state_.Reset(clearCinematic); }
	void InitializeFlightDirection(const Vector3& forward) { state_.InitializeFlightDirection(forward); }

	void UpdateSequence(
		Player* player,
		std::list<std::unique_ptr<Enemy>>& enemies,
		std::vector<EnemySpawnData>& enemySpawns,
		EnemySpawnManager& enemySpawnManager,
		BossEncounterState& bossEncounterState,
		const std::function<void()>& clearAllEnemyReferences);

	void UpdatePilot(Player* player) const;

	bool IsClearCinematic() const { return state_.IsClearCinematic(); }
	const AttractDemoState& GetState() const { return state_; }

private:
	AttractDemoState state_;
};
