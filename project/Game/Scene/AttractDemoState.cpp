#include "AttractDemoState.h"

#include <cmath>

void AttractDemoState::Reset(bool playClearCinematic) {
	clearCinematic_ = playClearCinematic;
	bossCreated_ = false;
	frame_ = 0;
	flightDirection_ = { 0.0f, 0.0f, 1.0f };
	flightYaw_ = 0.0f;
}

void AttractDemoState::InitializeFlightDirection(const Vector3& forward) {
	flightDirection_ = forward;
	flightDirection_.y = 0.0f;
	const float horizontalLength = std::sqrt(
		flightDirection_.x * flightDirection_.x + flightDirection_.z * flightDirection_.z);
	if (horizontalLength > 0.0001f) {
		flightDirection_.x /= horizontalLength;
		flightDirection_.z /= horizontalLength;
	} else {
		flightDirection_ = { 0.0f, 0.0f, 1.0f };
	}
	flightYaw_ = std::atan2(flightDirection_.x, flightDirection_.z);
}

void AttractDemoState::AdvanceFrame() {
	++frame_;
}
