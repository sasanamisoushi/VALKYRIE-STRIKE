#include "PlayerEnemyContactResolver.h"

#include "Game/Player/Player.h"
#include "Game/enemy/Enemy.h"

#include <algorithm>
#include <cmath>

bool PlayerEnemyContactResolver::Resolve(Player& player, const Enemy& enemy) const {
	const Vector3 playerPosition = player.GetPosition();
	const Vector3 enemyPosition = enemy.GetPosition();
	const Vector3 separation = {
		playerPosition.x - enemyPosition.x,
		playerPosition.y - enemyPosition.y,
		playerPosition.z - enemyPosition.z,
	};
	const float minimumDistance = player.GetCollisionRadius() + enemy.GetCollisionRadius();
	const float distanceSquared = separation.x * separation.x + separation.y * separation.y + separation.z * separation.z;
	if (distanceSquared >= minimumDistance * minimumDistance) {
		return false;
	}

	const float distance = std::sqrt((std::max)(distanceSquared, 0.0001f));
	const Vector3 pushDirection = distanceSquared > 0.0001f
		? Vector3{ separation.x / distance, separation.y / distance, separation.z / distance }
		: player.GetForwardVector();
	constexpr float kContactSeparationMargin = 0.03f;
	const float pushDistance = minimumDistance - distance + kContactSeparationMargin;
	player.SetPosition({
		playerPosition.x + pushDirection.x * pushDistance,
		playerPosition.y + pushDirection.y * pushDistance,
		playerPosition.z + pushDirection.z * pushDistance,
	});
	return true;
}
