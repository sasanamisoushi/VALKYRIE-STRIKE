#pragma once

#include "engine/math/MyMath.h"
#include <list>
#include <memory>

class GroundEnemy;
class Obstacle;

// 地上敵の地形接地・歩行／履帯移動・斜面姿勢追従・経路移動を担当する。
class GroundEnemyMovementController {
public:
	void UpdateGroundMovement(
		GroundEnemy& enemy,
		const Vector3& playerPos,
		const std::list<std::unique_ptr<Obstacle>>& obstacles) const;
	void UpdateTerrainPose(
		GroundEnemy& enemy,
		const std::list<std::unique_ptr<Obstacle>>& obstacles) const;
	void SnapToGround(
		GroundEnemy& enemy,
		const std::list<std::unique_ptr<Obstacle>>& obstacles) const;
	float GetFootOffset(const GroundEnemy& enemy) const;
	void UpdateFlightPathMovement(GroundEnemy& enemy) const;
};
