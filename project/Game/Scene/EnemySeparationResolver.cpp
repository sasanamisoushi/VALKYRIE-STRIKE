#include "EnemySeparationResolver.h"

#include "Game/enemy/Enemy.h"

#include <cmath>
#include <iterator>

void EnemySeparationResolver::Resolve(std::list<std::unique_ptr<Enemy>>& enemies) const {
	for (auto first = enemies.begin(); first != enemies.end(); ++first) {
		if (!(*first) || (*first)->IsDead()) {
			continue;
		}

		const Vector3 firstPosition = (*first)->GetPosition();
		float firstRadius = (*first)->GetCollisionRadius();
		if (firstRadius <= 0.1f) {
			firstRadius = 1.2f;
		}

		for (auto second = std::next(first); second != enemies.end(); ++second) {
			if (!(*second) || (*second)->IsDead() || (*first)->IsBoss() || (*second)->IsBoss()) {
				continue;
			}

			const Vector3 secondPosition = (*second)->GetPosition();
			float secondRadius = (*second)->GetCollisionRadius();
			if (secondRadius <= 0.1f) {
				secondRadius = 1.2f;
			}

			const Vector3 difference = { firstPosition.x - secondPosition.x, 0.0f, firstPosition.z - secondPosition.z };
			const float distanceSquared = difference.x * difference.x + difference.z * difference.z;
			const float minimumDistance = firstRadius + secondRadius + 3.0f;
			if (distanceSquared >= minimumDistance * minimumDistance) {
				continue;
			}

			const float distance = std::sqrt(distanceSquared);
			Vector3 direction = { 1.0f, 0.0f, 0.0f };
			if (distance > 0.001f) {
				direction = { difference.x / distance, 0.0f, difference.z / distance };
			}
			const float overlap = (minimumDistance - distance) * 0.5f;
			(*first)->SetPosition({
				firstPosition.x + direction.x * overlap,
				firstPosition.y,
				firstPosition.z + direction.z * overlap,
			});
			(*second)->SetPosition({
				secondPosition.x - direction.x * overlap,
				secondPosition.y,
				secondPosition.z - direction.z * overlap,
			});
		}
	}
}
