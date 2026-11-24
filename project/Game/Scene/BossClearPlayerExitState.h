#pragma once
#include "engine/math/MyMath.h"

class BossClearPlayerExitState {
public:
	void Reset();
	void Set(const Vector3& position, const Vector3& direction, float speed, float roll);
	Vector3& Position() { return position_; }
	const Vector3& GetPosition() const { return position_; }
	Vector3& Direction() { return direction_; }
	const Vector3& GetDirection() const { return direction_; }
	float& Speed() { return speed_; }
	float GetSpeed() const { return speed_; }
	float& Roll() { return roll_; }
	float GetRoll() const { return roll_; }
private:
	Vector3 position_{};
	Vector3 direction_ = {0.0f, 0.0f, 1.0f};
	float speed_ = 0.0f;
	float roll_ = 0.0f;
};
