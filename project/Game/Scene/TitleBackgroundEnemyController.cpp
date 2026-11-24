#include "TitleBackgroundEnemyController.h"

#include "TitleBackgroundEnemyState.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Enemy.h"

Enemy* TitleBackgroundEnemyController::AcquireTarget(Player& player,
	std::list<std::unique_ptr<Enemy>>& enemies, TitleBackgroundEnemyState& state) const {
	for (const auto& enemy : enemies) {
		if (enemy && !enemy->IsDead()) {
			return enemy.get();
		}
	}

	if (state.IsRespawnPending()) {
		state.AdvanceRespawnTimer();
		return nullptr;
	}

	auto enemy = std::make_unique<Enemy>();
	enemy->Initialize(player.GetPosition());
	Enemy* target = enemy.get();
	enemies.push_back(std::move(enemy));
	state.BeginEntering();
	return target;
}
