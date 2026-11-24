#include "SpecialAttackState.h"

void SpecialAttackState::Reset() {
	gauge_ = kMaximumGauge;
	active_ = false;
	frame_ = 0;
}

bool SpecialAttackState::CanStart(float cost) const {
	return !active_ && gauge_ >= cost;
}

bool SpecialAttackState::TryStart(float cost) {
	if (!CanStart(cost)) {
		return false;
	}
	gauge_ -= cost;
	active_ = true;
	frame_ = 0;
	return true;
}

bool SpecialAttackState::IsFireFrame(int intervalFrames) const {
	return active_ && intervalFrames > 0 && frame_ % intervalFrames == 0;
}

bool SpecialAttackState::Advance(int durationFrames) {
	if (!active_) {
		return false;
	}
	++frame_;
	if (frame_ < durationFrames) {
		return false;
	}
	active_ = false;
	frame_ = 0;
	return true;
}

void SpecialAttackState::Recover(float amount) {
	if (!active_) {
		gauge_ = std::clamp(gauge_ + amount, 0.0f, kMaximumGauge);
	}
}
