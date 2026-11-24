#include "BossClearEffectState.h"

void BossClearEffectState::Reset() {
	smokeSeconds_ = 0.0f;
	phase3Initialized_ = false;
	missionClearUiStarted_ = false;
	missionClearUiSeconds_ = 0.0f;
}

void BossClearEffectState::ResetSmokeTimer() {
	smokeSeconds_ = 0.0f;
}

bool BossClearEffectState::ShouldEmitSmoke(float deltaSeconds, float intervalSeconds) {
	smokeSeconds_ += deltaSeconds;
	if (smokeSeconds_ < intervalSeconds) {
		return false;
	}
	smokeSeconds_ = 0.0f;
	return true;
}

bool BossClearEffectState::TryInitializePhase3() {
	if (phase3Initialized_) {
		return false;
	}
	phase3Initialized_ = true;
	return true;
}

void BossClearEffectState::StartMissionClearUi() {
	missionClearUiStarted_ = true;
	missionClearUiSeconds_ = 0.0f;
}

void BossClearEffectState::AdvanceMissionClearUi(float deltaSeconds) {
	if (missionClearUiStarted_) {
		missionClearUiSeconds_ += deltaSeconds;
	}
}
