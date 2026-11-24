#pragma once

#include "engine/Utility/StageLoader.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

class BossEncounterState;
class Enemy;
class EnemyEventManager;
class EnemySpawnManager;
class LockOnManager;
class Obstacle;
class Player;
class TitleBackgroundEnemyState;

// ステージ JSON（scene.json / enemy_events.json）の読み込み・再構築と、
// 編集モード時のホットリロード監視を管理する。
class StageReloadController {
public:
	static constexpr const char* kSceneJsonPath = "resources/scene.json";
	static constexpr const char* kEnemyEventsJsonPath = "resources/enemy_events.json";

	void Reload(
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
		const std::function<void()>& clearAllEnemyReferences);

	// 監視が有効なとき、一定間隔で scene.json の更新を検知したら true を返す。
	bool CheckHotReload(bool enabled);

	// 外部保存直後にタイムスタンプを同期し、不要な再読込を防ぐ。
	void SyncWriteTime(const std::string& filePath = kSceneJsonPath);

private:
	std::filesystem::file_time_type lastWriteTime_{};
	uint32_t checkFrameCounter_ = 0;
};
