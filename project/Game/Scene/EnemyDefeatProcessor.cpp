#include "EnemyDefeatProcessor.h"

#include "EnemySpawnManager.h"
#include "SongState.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyEventManager.h"

void EnemyDefeatProcessor::ProcessStandardDefeat(const Enemy& defeatedEnemy,
	std::vector<EnemySpawnData>& spawnData, const EnemyEventManager& eventManager,
	EnemySpawnManager& spawnManager, const std::list<std::unique_ptr<Enemy>>& activeEnemies,
	SongState& songState) const {
	const size_t spawnPointIndex = defeatedEnemy.GetSpawnPointIndex();
	if (spawnPointIndex < spawnData.size()) {
		spawnManager.TriggerReinforcements(
			spawnData[spawnPointIndex].name, spawnData, eventManager, activeEnemies);
	}
	songState.AddGauge(kSongGaugeReward);
}
