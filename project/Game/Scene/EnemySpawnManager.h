#pragma once

#include "engine/Utility/StageLoader.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

class Enemy;
class EnemyEventManager;
class Obstacle;

// ステージに定義された敵の生成、増援条件、再出現タイマーを管理する。
// GamePlayScene はステージデータと敵の所有権を渡すだけで、スポーン手順の詳細を持たない。
class EnemySpawnManager {
public:
	void Reset(const std::vector<EnemySpawnData>& spawnData);
	void Clear();

	void SpawnFromPoint(size_t spawnPointIndex, std::vector<EnemySpawnData>& spawnData,
		std::list<std::unique_ptr<Enemy>>& enemies, std::list<std::unique_ptr<Obstacle>>& obstacles);
	void TriggerReinforcements(const std::string& deadEnemyName, std::vector<EnemySpawnData>& spawnData,
		const EnemyEventManager& eventManager, const std::list<std::unique_ptr<Enemy>>& enemies);
	void Update(std::vector<EnemySpawnData>& spawnData, std::list<std::unique_ptr<Enemy>>& enemies,
		std::list<std::unique_ptr<Obstacle>>& obstacles);
	bool HasPendingSpawns() const;

private:
	bool IsSpawnPointActive(size_t spawnPointIndex, const std::list<std::unique_ptr<Enemy>>& enemies) const;
	void ScheduleSpawn(size_t spawnPointIndex, int delayFrames, const std::vector<EnemySpawnData>& spawnData,
		const std::list<std::unique_ptr<Enemy>>& enemies);

	std::vector<int> respawnTimers_;
};
