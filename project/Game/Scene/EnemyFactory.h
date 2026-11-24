#pragma once

#include "engine/Utility/StageLoader.h"

#include <cstddef>
#include <list>
#include <memory>

class Enemy;
class Obstacle;

// ステージ定義に応じた敵の生成と、生成直後に必要な共通初期化を一箇所に集約する。
class EnemyFactory {
public:
	std::unique_ptr<Enemy> Create(const EnemySpawnData& spawnData, size_t spawnPointIndex,
		const std::list<std::unique_ptr<Obstacle>>& obstacles) const;
	std::unique_ptr<Enemy> CreateGroundEnemy(const Vector3& position,
		const std::list<std::unique_ptr<Obstacle>>& obstacles) const;

private:
	std::unique_ptr<Enemy> CreateByType(const EnemySpawnData& spawnData) const;
	void InitializeFromSpawnData(Enemy& enemy, const EnemySpawnData& spawnData, size_t spawnPointIndex,
		const std::list<std::unique_ptr<Obstacle>>& obstacles) const;
};
