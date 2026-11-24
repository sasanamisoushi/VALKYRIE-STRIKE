#include "BossClearTimelineState.h"

#include <algorithm>

void BossClearTimelineState::Reset() {
	timerInitialized_ = false;
	sequenceSeconds_ = 0.0f;
	phaseSeconds_ = 0.0f;
	cinematicFrames_ = 0;
}

void BossClearTimelineState::Pause() {
	timerInitialized_ = false;
}

float BossClearTimelineState::Advance(std::chrono::steady_clock::time_point now) {
	float deltaSeconds = 1.0f / 60.0f;
	if (timerInitialized_) {
		deltaSeconds = std::chrono::duration<float>(now - lastTickTime_).count();
		deltaSeconds = (std::clamp)(deltaSeconds, 0.0f, 0.05f);
	}
	lastTickTime_ = now;
	timerInitialized_ = true;
	sequenceSeconds_ += deltaSeconds;
	phaseSeconds_ += deltaSeconds;
	++cinematicFrames_;
	return deltaSeconds;
}

void BossClearTimelineState::ResetPhase() {
	phaseSeconds_ = 0.0f;
}

void BossClearTimelineState::ResetCinematicFrames() {
	cinematicFrames_ = 0;
}
