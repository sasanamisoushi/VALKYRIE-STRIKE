#include "GameOverSequence.h"

void GameOverSequence::Reset() {
	active_ = false;
	frame_ = 0;
	focusPosition_ = {};
}

bool GameOverSequence::Start(const Vector3& focusPosition) {
	if (active_) {
		return false;
	}
	active_ = true;
	frame_ = 0;
	focusPosition_ = focusPosition;
	return true;
}

void GameOverSequence::Advance() {
	if (active_) {
		++frame_;
	}
}
