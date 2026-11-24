#include "BossClearCameraState.h"

#include <algorithm>

namespace {
constexpr float kMinDirectionLength = 0.001f;
constexpr float kZoomSpeedPerSec = 2.2f;
constexpr float kSlideSpeedPerSec = 2.8f;
constexpr float kVerticalLiftPerSec = 0.45f;
constexpr float kBaseFovY = 0.70f;
constexpr float kMinFovY = 0.54f;
constexpr float kFovShrinkPerSec = 0.025f;
}

void BossClearCameraState::Reset() {
	explosionCenter_ = { 0.0f, 0.0f, 0.0f };
	anchorPosition_ = { 0.0f, 0.0f, 0.0f };
	slideDirection_ = { 1.0f, 0.0f, 0.0f };
}

Vector3 BossClearCameraState::ComputeForwardDirection() const {
	const Vector3 toExplosion = {
		explosionCenter_.x - anchorPosition_.x,
		explosionCenter_.y - anchorPosition_.y,
		explosionCenter_.z - anchorPosition_.z,
	};
	const float length = MyMath::Length(toExplosion);
	if (length <= kMinDirectionLength) {
		return { 0.0f, 0.0f, 1.0f };
	}
	return { toExplosion.x / length, toExplosion.y / length, toExplosion.z / length };
}

Vector3 BossClearCameraState::ComputeSlideDirection() const {
	if (MyMath::Length(slideDirection_) <= kMinDirectionLength) {
		return { 1.0f, 0.0f, 0.0f };
	}
	return MyMath::Normalize(slideDirection_);
}

Vector3 BossClearCameraState::RefreshSlideDirectionFromAnchor() {
	const Vector3 forward = ComputeForwardDirection();
	Vector3 right = MyMath::Cross({ 0.0f, 1.0f, 0.0f }, forward);
	if (MyMath::Length(right) < kMinDirectionLength) {
		right = { 1.0f, 0.0f, 0.0f };
	} else {
		right = MyMath::Normalize(right);
	}
	slideDirection_ = right;
	return slideDirection_;
}

Vector3 BossClearCameraState::EvaluateAftermathCameraPosition(float phaseElapsedSeconds) const {
	const Vector3 forwardDir = ComputeForwardDirection();
	const Vector3 slideDir = ComputeSlideDirection();
	const float zoomDistance = phaseElapsedSeconds * kZoomSpeedPerSec;
	const float slideDistance = phaseElapsedSeconds * kSlideSpeedPerSec;
	const float verticalLift = phaseElapsedSeconds * kVerticalLiftPerSec;
	return {
		anchorPosition_.x + forwardDir.x * zoomDistance + slideDir.x * slideDistance,
		anchorPosition_.y + forwardDir.y * zoomDistance + slideDir.y * slideDistance + verticalLift,
		anchorPosition_.z + forwardDir.z * zoomDistance + slideDir.z * slideDistance,
	};
}

float BossClearCameraState::EvaluateAftermathFovY(float phaseElapsedSeconds) const {
	return (std::max)(kMinFovY, kBaseFovY - phaseElapsedSeconds * kFovShrinkPerSec);
}
