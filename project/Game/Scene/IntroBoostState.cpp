#include "IntroBoostState.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 6.28318531f;
constexpr float kPitchRadians = -0.05f;
constexpr float kVerticalWaveAmplitude = 0.02f;
constexpr float kThrusterScale = 2.2f;
}

void IntroBoostState::Reset(float yaw) {
	remainingFrames_ = 0;
	initialFrames_ = 0;
	yaw_ = yaw;
	velocity_ = { 0.0f, 0.0f, 0.0f };
}

void IntroBoostState::Start(int durationFrames, float yaw, const Vector3& velocity) {
	if (durationFrames <= 0) {
		Reset(yaw);
		return;
	}
	remainingFrames_ = durationFrames;
	initialFrames_ = durationFrames;
	yaw_ = yaw;
	velocity_ = velocity;
}

IntroBoostState::StepResult IntroBoostState::Advance(const Vector3& currentPosition) {
	StepResult result{};
	result.nextPosition = currentPosition;
	result.rotation = { kPitchRadians, yaw_, 0.0f };
	result.velocity = velocity_;
	result.thrusterScale = kThrusterScale;

	if (remainingFrames_ <= 0) {
		return result;
	}

	const float progress = 1.0f - static_cast<float>(remainingFrames_) /
		static_cast<float>((std::max)(1, initialFrames_));
	result.nextPosition.x += velocity_.x;
	result.nextPosition.y += velocity_.y + std::sin(progress * kPi) * kVerticalWaveAmplitude;
	result.nextPosition.z += velocity_.z;
	result.rotation.z = progress * kTwoPi;

	--remainingFrames_;
	return result;
}
