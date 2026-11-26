#pragma once

#include "engine/math/MyMath.h"
#include <vector>

class Boss;

// ボス撃破演出時の焼損表現・デブリ飛散・着地残骸管理を担当する。
class BossCinematicDebrisController {
public:
	void SetWreckPresentation(Boss& boss, float burnRatio) const;
	void Begin(Boss& boss) const;
	void SetRestPlane(Boss& boss, float worldY) const;
	void Step(Boss& boss, float deltaTime) const;
	std::vector<Vector3> GetWreckageWorldPositions(const Boss& boss) const;
	std::vector<Vector3> GetDebrisWorldPositions(const Boss& boss) const;
};
