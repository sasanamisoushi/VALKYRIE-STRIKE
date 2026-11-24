#include "SongState.h"

void SongState::Reset() {
	gauge_ = kMaximumGauge;
	active_ = false;
	frame_ = 0;
}

bool SongState::CanActivate() const {
	return !active_ && gauge_ >= kMaximumGauge;
}

bool SongState::TryActivate() {
	if (!CanActivate()) {
		return false;
	}
	gauge_ = 0.0f;
	active_ = true;
	frame_ = 0;
	return true;
}

bool SongState::Advance(int durationFrames) {
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

void SongState::AddGauge(float amount) {
	if (!active_) {
		gauge_ = std::clamp(gauge_ + amount, 0.0f, kMaximumGauge);
	}
}
