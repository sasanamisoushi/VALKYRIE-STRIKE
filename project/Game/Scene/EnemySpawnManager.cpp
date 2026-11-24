#include "EnemySpawnManager.h"

#include "EnemyFactory.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyEventManager.h"
#include "Game/obstacle/Obstacle.h"

#include <algorithm>

namespace {
	constexpr int kNoRespawnTimer = -1;
}

void EnemySpawnManager::Reset(const std::vector<EnemySpawnData>& spawnData) {
	respawnTimers_.assign(spawnData.size(), kNoRespawnTimer);
}

void EnemySpawnManager::Clear() {
	respawnTimers_.clear();
}

void EnemySpawnManager::SpawnFromPoint(size_t spawnPointIndex, std::vector<EnemySpawnData>& spawnData,
	std::list<std::unique_ptr<Enemy>>& enemies, std::list<std::unique_ptr<Obstacle>>& obstacles) {
	if (spawnPointIndex >= spawnData.size()) {
		return;
	}

	EnemySpawnData& point = spawnData[spawnPointIndex];
	point.hasSpawned = true;
	EnemyFactory factory;
	std::unique_ptr<Enemy> enemy = factory.Create(point, spawnPointIndex, obstacles);
	enemies.push_back(std::move(enemy));

	if (spawnPointIndex < respawnTimers_.size()) {
		respawnTimers_[spawnPointIndex] = kNoRespawnTimer;
	}
}

bool EnemySpawnManager::IsSpawnPointActive(size_t spawnPointIndex,
	const std::list<std::unique_ptr<Enemy>>& enemies) const {
	for (const auto& enemy : enemies) {
		if (enemy && enemy->GetSpawnPointIndex() == spawnPointIndex && !enemy->IsDead()) {
			return true;
		}
	}
	return false;
}

void EnemySpawnManager::ScheduleSpawn(size_t spawnPointIndex, int delayFrames,
	const std::vector<EnemySpawnData>& spawnData, const std::list<std::unique_ptr<Enemy>>& enemies) {
	if (spawnPointIndex >= spawnData.size() || spawnData[spawnPointIndex].hasSpawned) {
		return;
	}
	if (respawnTimers_.size() < spawnData.size()) {
		respawnTimers_.resize(spawnData.size(), kNoRespawnTimer);
	}
	if (respawnTimers_[spawnPointIndex] != kNoRespawnTimer || IsSpawnPointActive(spawnPointIndex, enemies)) {
		return;
	}
	respawnTimers_[spawnPointIndex] = delayFrames > 0 ? delayFrames : 1;
}

void EnemySpawnManager::TriggerReinforcements(const std::string& deadEnemyName,
	std::vector<EnemySpawnData>& spawnData, const EnemyEventManager& eventManager,
	const std::list<std::unique_ptr<Enemy>>& enemies) {
	if (deadEnemyName.empty()) {
		return;
	}

	for (const EnemyEvent& event : eventManager.GetEventsForTrigger(deadEnemyName)) {
		for (size_t index = 0; index < spawnData.size(); ++index) {
			if (spawnData[index].name == event.targetEnemyName) {
				ScheduleSpawn(index, event.delayFrames, spawnData, enemies);
				break;
			}
		}
	}

	for (size_t index = 0; index < spawnData.size(); ++index) {
		EnemySpawnData& point = spawnData[index];
		auto trigger = std::find(point.remainingReinforcementTriggers.begin(),
			point.remainingReinforcementTriggers.end(), deadEnemyName);
		if (trigger == point.remainingReinforcementTriggers.end()) {
			continue;
		}
		point.remainingReinforcementTriggers.erase(trigger);
		if (point.remainingReinforcementTriggers.empty()) {
			ScheduleSpawn(index, point.reinforcementDelayFrames, spawnData, enemies);
		}
	}
}

void EnemySpawnManager::Update(std::vector<EnemySpawnData>& spawnData,
	std::list<std::unique_ptr<Enemy>>& enemies, std::list<std::unique_ptr<Obstacle>>& obstacles) {
	if (respawnTimers_.size() < spawnData.size()) {
		respawnTimers_.resize(spawnData.size(), kNoRespawnTimer);
	}
	for (size_t index = 0; index < respawnTimers_.size(); ++index) {
		int& timer = respawnTimers_[index];
		if (timer == kNoRespawnTimer) {
			continue;
		}
		if (timer > 0) {
			--timer;
		}
		if (timer == 0) {
			SpawnFromPoint(index, spawnData, enemies, obstacles);
		}
	}
}

bool EnemySpawnManager::HasPendingSpawns() const {
	return std::any_of(respawnTimers_.begin(), respawnTimers_.end(), [](int timer) {
		return timer != kNoRespawnTimer;
	});
}
