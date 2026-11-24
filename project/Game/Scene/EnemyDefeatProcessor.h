#pragma once

#include "engine/Utility/StageLoader.h"

#include <list>
#include <memory>
#include <vector>

class Enemy;
class EnemyEventManager;
class EnemySpawnManager;
class SongState;

// 通常敵の撃破報酬と、撃破を起点にした増援イベントを処理する。
class EnemyDefeatProcessor {
public:
	void ProcessStandardDefeat(const Enemy& defeatedEnemy, std::vector<EnemySpawnData>& spawnData,
		const EnemyEventManager& eventManager, EnemySpawnManager& spawnManager,
		const std::list<std::unique_ptr<Enemy>>& activeEnemies, SongState& songState) const;

private:
	static constexpr float kSongGaugeReward = 20.0f;
};
