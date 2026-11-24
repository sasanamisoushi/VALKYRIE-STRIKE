#pragma once

#include "3D/Object3d.h"

#include <memory>

class Player;
struct BoundaryShieldSettings;

// ステージ境界（壁面・天井）接近時の六角形エネルギーシールド演出を管理・描画する。
class BoundaryShieldRenderer {
public:
	void Initialize();
	void Draw(bool enabled, const Player* player, const BoundaryShieldSettings& settings);

private:
	void DrawAlertSurface(
		Object3d* alertObject,
		const Vector3& position,
		const Vector3& normal,
		float intensity,
		float pulseAlpha,
		const BoundaryShieldSettings& settings) const;

	std::unique_ptr<Object3d> boundaryAlertObject_;
	std::unique_ptr<Object3d> ceilingBoundaryAlertObject_;
	float pulseTime_ = 0.0f;
};
