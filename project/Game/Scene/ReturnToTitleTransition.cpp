#include "ReturnToTitleTransition.h"

void ReturnToTitleTransition::Reset() {
	active_ = false;
	frame_ = 0;
}

bool ReturnToTitleTransition::Start() {
	if (active_) {
		return false;
	}
	active_ = true;
	frame_ = 0;
	return true;
}

bool ReturnToTitleTransition::Advance() {
	if (!active_) {
		return false;
	}
	++frame_;
	return frame_ >= kDurationFrames;
}
