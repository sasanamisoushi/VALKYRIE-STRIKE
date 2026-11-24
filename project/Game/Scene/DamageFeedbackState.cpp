#include "DamageFeedbackState.h"

void DamageFeedbackState::Reset() {
	previousPlayerHP_ = kUninitializedHP;
	effectTimer_ = 0;
}

void DamageFeedbackState::ObservePlayerHP(int currentHP) {
	if (previousPlayerHP_ == kUninitializedHP) {
		previousPlayerHP_ = currentHP;
		return;
	}
	if (currentHP < previousPlayerHP_) {
		effectTimer_ = kDefaultEffectDurationFrames;
	}
	previousPlayerHP_ = currentHP;
}

bool DamageFeedbackState::ConsumeEffectFrame() {
	if (effectTimer_ <= 0) {
		return false;
	}
	--effectTimer_;
	return true;
}
