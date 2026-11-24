#pragma once

#include <list>
#include <memory>

class Boss;
class Enemy;

// ボスが要求した護衛機を、同時出現数の上限を守って生成する。
class BossMinionSpawner {
public:
	void SpawnRequestedMinions(Boss& boss, int maximumActiveMinions,
		std::list<std::unique_ptr<Enemy>>& enemies) const;
};
