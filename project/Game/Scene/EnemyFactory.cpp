#include "EnemyFactory.h"

#include "Game/enemy/Boss.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/GroundEnemy.h"
#include "Game/enemy/JammerEnemy.h"
#include "Game/obstacle/Obstacle.h"

std::unique_ptr<Enemy> EnemyFactory::Create(const EnemySpawnData& spawnData, size_t spawnPointIndex,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	std::unique_ptr<Enemy> enemy = CreateByType(spawnData);
	InitializeFromSpawnData(*enemy, spawnData, spawnPointIndex, obstacles);
	return enemy;
}

std::unique_ptr<Enemy> EnemyFactory::CreateGroundEnemy(const Vector3& position,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	EnemySpawnData spawnData{};
	spawnData.position = position;
	spawnData.isGround = true;
	return Create(spawnData, Enemy::kNoSpawnPoint, obstacles);
}

std::unique_ptr<Enemy> EnemyFactory::CreateByType(const EnemySpawnData& spawnData) const {
	if (spawnData.isBoss) {
		return std::make_unique<Boss>();
	}
	if (spawnData.isJammer) {
		return std::make_unique<JammerEnemy>();
	}
	if (spawnData.isGround) {
		return std::make_unique<GroundEnemy>();
	}
	return std::make_unique<Enemy>();
}

void EnemyFactory::InitializeFromSpawnData(Enemy& enemy, const EnemySpawnData& spawnData, size_t spawnPointIndex,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	enemy.Initialize(spawnData.position);
	enemy.SetRotation(spawnData.rotation);
	if (spawnData.flightPath.IsValid()) {
		enemy.SetFlightPath(spawnData.flightPath.points, spawnData.flightPath.loop, spawnData.flightPath.speed);
	}
	enemy.SetSpawnPointIndex(spawnPointIndex);
	enemy.OnSpawned(obstacles);
}
