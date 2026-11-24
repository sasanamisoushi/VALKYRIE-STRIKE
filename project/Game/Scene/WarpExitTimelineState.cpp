#include "WarpExitTimelineState.h"

void WarpExitTimelineState::Reset() {
	active_ = false;
	impactTriggered_ = false;
	frame_ = 0;
	gateFrames_ = 0;
}

void WarpExitTimelineState::Start() {
	active_ = true;
	impactTriggered_ = false;
	frame_ = 0;
	gateFrames_ = kGateLifetimeFrames;
}

void WarpExitTimelineState::AdvanceFrame() {
	if (active_) {
		++frame_;
	}
}

void WarpExitTimelineState::Finish() {
	active_ = false;
}

bool WarpExitTimelineState::TryTriggerImpact() {
	if (impactTriggered_) {
		return false;
	}
	impactTriggered_ = true;
	return true;
}

void WarpExitTimelineState::HideGate() {
	gateFrames_ = 0;
}

void WarpExitTimelineState::AdvanceGateFrame() {
	if (gateFrames_ > 0) {
		--gateFrames_;
	}
}
