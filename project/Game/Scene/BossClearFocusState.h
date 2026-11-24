#pragma once

#include "engine/math/MyMath.h"

// ボス撃破演出で共有する注視点と演出スケールを管理する。
class BossClearFocusState {
public:
	void Reset();
	void Set(const Vector3& position, float radius);
	void SetPosition(const Vector3& position) { position_ = position; }

	const Vector3& GetPosition() const { return position_; }
	float GetRadius() const { return radius_; }

private:
	Vector3 position_{};
	float radius_ = 1.0f;
};
