#include "BossSpawnController.h"

#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h"

#include <algorithm>

void BossSpawnController::Spawn(const Vector3& requestedPosition, const Boss::Tuning* tuning,
	const std::list<std::unique_ptr<Obstacle>>& obstacles,
	std::list<std::unique_ptr<Enemy>>& enemies) const {
	auto boss = std::make_unique<Boss>();
	boss->Initialize(requestedPosition);
	if (tuning) {
		boss->SetTuning(*tuning);
	}

	if (const auto stageBoundsIt = std::find_if(obstacles.begin(), obstacles.end(),
		[](const std::unique_ptr<Obstacle>& obstacle) { return obstacle && obstacle->IsStageBounds(); });
		stageBoundsIt != obstacles.end()) {
		const Obstacle& stageBounds = **stageBoundsIt;
		const Vector3 center = stageBounds.GetPosition();
		const Vector3 halfExtents = stageBounds.GetWorldHalfExtents();
		const Vector3 bossHalfExtents = boss->GetWorldHalfExtents();
		constexpr float kBoundaryMargin = 2.0f;
		auto clampInsideStage = [](float desired, float centerValue, float stageHalfExtent, float bossHalfExtent) {
			const float minimum = centerValue - stageHalfExtent + bossHalfExtent + kBoundaryMargin;
			const float maximum = centerValue + stageHalfExtent - bossHalfExtent - kBoundaryMargin;
			return minimum <= maximum ? std::clamp(desired, minimum, maximum) : centerValue;
		};

		boss->SetPosition({
			clampInsideStage(requestedPosition.x, center.x, halfExtents.x, bossHalfExtents.x),
			clampInsideStage(requestedPosition.y, center.y, halfExtents.y, bossHalfExtents.y),
			clampInsideStage(requestedPosition.z, center.z, halfExtents.z, bossHalfExtents.z),
		});
	}

	boss->UpdateModel();
	enemies.push_back(std::move(boss));
}
