#pragma once

#include "engine/math/MyMath.h"

// ステージ開始直後の出撃ブースト演出の状態と移動計算を管理する。
class IntroBoostState {
public:
	struct StepResult {
		Vector3 nextPosition{};
		Vector3 rotation{};
		Vector3 velocity{};
		float thrusterScale = 2.2f;
	};

	void Reset(float yaw = 0.0f);
	void Start(int durationFrames, float yaw, const Vector3& velocity);
	StepResult Advance(const Vector3& currentPosition);

	bool IsActive() const { return remainingFrames_ > 0; }
	int GetRemainingFrames() const { return remainingFrames_; }
	float GetYaw() const { return yaw_; }
	const Vector3& GetVelocity() const { return velocity_; }

private:
	int remainingFrames_ = 0;
	int initialFrames_ = 0;
	float yaw_ = 0.0f;
	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
};
