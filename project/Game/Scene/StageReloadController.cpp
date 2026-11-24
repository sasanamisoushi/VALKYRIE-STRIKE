#include "StageReloadController.h"

#include "BossEncounterState.h"
#include "EnemySpawnManager.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyEventManager.h"
#include "Game/obstacle/Obstacle.h"
#include "LockOnManager.h"
#include "TitleBackgroundEnemyState.h"

#include <algorithm>

void StageReloadController::Reload(
	bool isAutoFlightMode,
	Player* player,
	std::list<std::unique_ptr<Enemy>>& enemies,
	std::list<std::unique_ptr<Obstacle>>& obstacles,
	std::vector<EnemySpawnData>& enemySpawns,
	BossEncounterState& bossEncounterState,
	EnemySpawnManager& enemySpawnManager,
	EnemyEventManager& enemyEventManager,
	TitleBackgroundEnemyState& titleBackgroundEnemyState,
	LockOnManager* lockOnManager,
	const std::function<void()>& clearAllEnemyReferences) {
	bossEncounterState.Reset();
	if (clearAllEnemyReferences) {
		clearAllEnemyReferences();
	}
	if (lockOnManager) {
		lockOnManager->CancelMultiLock();
	}
	enemies.clear();
	obstacles.clear();
	enemySpawns.clear();

	StageLoader::LoadSceneJson(kSceneJsonPath, enemies, obstacles, player, &enemySpawns);
	if (isAutoFlightMode) {
		// タイトル背景は戦闘の見せ場用なので、VF1 以外や増援条件付きの敵を持ち込まない。
		// VF1 は最初から出しておき、タイトルを開いた瞬間からロックオン対象を作る。
		enemySpawns.erase(
			std::remove_if(enemySpawns.begin(), enemySpawns.end(), [](const EnemySpawnData& spawn) {
				return spawn.enemyType != "VF1";
			}),
			enemySpawns.end());
		for (EnemySpawnData& spawn : enemySpawns) {
			spawn.isInitialSpawn = true;
			spawn.hasSpawned = false;
			spawn.reinforcementTriggerNames.clear();
			spawn.remainingReinforcementTriggers.clear();
		}
		titleBackgroundEnemyState.Reset();
	}
	enemySpawnManager.Reset(enemySpawns);

	enemyEventManager.LoadEvents(kEnemyEventsJsonPath);
	// Blender/StageLoaderで設定された初期スポーン設定(isInitialSpawn)をそのまま尊重する

	// 障害物のメッシュコライダー等を事前に構築・更新（敵の着地スナップ前に必須）
	for (auto& obstacle : obstacles) {
		if (obstacle) {
			obstacle->Update();
		}
	}

	for (size_t i = 0; i < enemySpawns.size(); ++i) {
		if (enemySpawns[i].isInitialSpawn) {
			enemySpawnManager.SpawnFromPoint(i, enemySpawns, enemies, obstacles);
		}
	}

	SyncWriteTime(kSceneJsonPath);
}

bool StageReloadController::CheckHotReload(bool enabled) {
	if (!enabled || ++checkFrameCounter_ % 30 != 0) {
		return false;
	}
	std::error_code ec;
	const auto currentTime = std::filesystem::last_write_time(kSceneJsonPath, ec);
	if (ec) {
		return false;
	}
	return currentTime > lastWriteTime_;
}

void StageReloadController::SyncWriteTime(const std::string& filePath) {
	std::error_code ec;
	const auto writeTime = std::filesystem::last_write_time(filePath, ec);
	if (!ec) {
		lastWriteTime_ = writeTime;
	}
}
