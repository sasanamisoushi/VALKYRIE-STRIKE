#pragma once
#include "engine/math/MyMath.h"

// ボス墜落時の初期姿勢と速度を管理する。
class BossClearCrashState {
public:
	void Reset();
	void Start(const Vector3& initialRotation, const Vector3& velocity);
	Vector3& Velocity() { return velocity_; }
	const Vector3& GetVelocity() const { return velocity_; }
	const Vector3& GetInitialRotation() const { return initialRotation_; }
private:
	Vector3 initialRotation_{};
	Vector3 velocity_{};
};
