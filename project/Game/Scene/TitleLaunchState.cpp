#include "TitleLaunchState.h"

#include <cmath>

void TitleLaunchState::Reset() {
	active_ = false;
	frame_ = 0;
	startPosition_ = {};
	forward_ = { 0.0f, 0.0f, 1.0f };
}

bool TitleLaunchState::Start(const Vector3& startPosition, const Vector3& forward) {
	if (active_) {
		return false;
	}

	startPosition_ = startPosition;
	forward_ = forward;
	forward_.y = 0.0f;
	const float horizontalLength = std::sqrt(forward_.x * forward_.x + forward_.z * forward_.z);
	if (horizontalLength > 0.0001f) {
		forward_.x /= horizontalLength;
		forward_.z /= horizontalLength;
	} else {
		forward_ = { 0.0f, 0.0f, 1.0f };
	}

	active_ = true;
	frame_ = 0;
	return true;
}

void TitleLaunchState::AdvanceFrame() {
	if (active_) {
		++frame_;
	}
}
