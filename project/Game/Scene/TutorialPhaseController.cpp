#include "TutorialPhaseController.h"

#include "TutorialProgressTracker.h"
#include "TutorialRequirementsManager.h"

void TutorialPhaseController::Reset(bool enabled) {
	enabled_ = enabled;
	state_ = CreateTutorialState(enabled ? TutorialPhase::Flight : TutorialPhase::Disabled);
	phaseFrames_ = 0;
	combatTargetsSpawned_ = false;
}

void TutorialPhaseController::TransitionTo(TutorialPhase next) {
	state_ = CreateTutorialState(next);
	phaseFrames_ = 0;
}

TutorialPhaseResult TutorialPhaseController::Update(const TutorialPhaseInput& input,
	const TutorialRequirements& requirements, TutorialProgressTracker& progress) {
	TutorialPhaseResult result;
	if (!enabled_ || !state_ || state_->GetPhase() == TutorialPhase::Disabled) {
		return result;
	}
	if (input.skipRequested) {
		TransitionTo(TutorialPhase::Disabled);
		result.clearGoal = true;
		return result;
	}

	++phaseFrames_;
	TutorialPhaseInput stateInput = input;
	stateInput.combatTargetsDefeated = combatTargetsSpawned_ && input.combatTargetsDefeated;
	const TutorialPhase next = state_->Update(stateInput, requirements, progress, result);
	if (next != state_->GetPhase()) {
		TransitionTo(next);
	}
	return result;
}
