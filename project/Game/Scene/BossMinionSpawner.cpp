#include "BossMinionSpawner.h"

#include "Game/enemy/Boss.h"
#include "Game/enemy/Enemy.h"

#include <algorithm>

void BossMinionSpawner::SpawnRequestedMinions(Boss& boss, int maximumActiveMinions,
	std::list<std::unique_ptr<Enemy>>& enemies) const {
	const int requestedCount = boss.ConsumeSummonRequests();
	const int activeMinionCount = static_cast<int>(std::count_if(
		enemies.begin(), enemies.end(),
		[](const std::unique_ptr<Enemy>& enemy) {
			return enemy && !enemy->IsDead() && enemy->IsBossMinion();
		}));
	const int summonCount = (std::min)(requestedCount, (std::max)(0, maximumActiveMinions - activeMinionCount));
	for (int index = 0; index < summonCount; ++index) {
		auto escort = std::make_unique<Enemy>();
		escort->Initialize(boss.GetPosition());
		escort->SetIsBossMinion(true);
		escort->StartChasingPlayer();
		enemies.push_back(std::move(escort));
	}
}
