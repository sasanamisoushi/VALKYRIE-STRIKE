#include "PauseMenuState.h"

void PauseMenuState::Reset() {
	paused_ = false;
	titleSelected_ = false;
}

bool PauseMenuState::Toggle() {
	paused_ = !paused_;
	if (paused_) {
		titleSelected_ = false;
	}
	return paused_;
}

void PauseMenuState::Resume() {
	paused_ = false;
}

void PauseMenuState::ToggleSelection() {
	if (paused_) {
		titleSelected_ = !titleSelected_;
	}
}
