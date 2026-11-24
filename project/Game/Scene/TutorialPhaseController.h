#pragma once

#include "TutorialState.h"

#include <memory>

class TutorialProgressTracker;
struct TutorialRequirements;

class TutorialPhaseController {
public:
	void Reset(bool enabled);
	TutorialPhaseResult Update(const TutorialPhaseInput& input, const TutorialRequirements& requirements,
		TutorialProgressTracker& progress);

	bool IsEnabled() const { return enabled_; }
	TutorialPhase GetPhase() const { return state_ ? state_->GetPhase() : TutorialPhase::Disabled; }
	int GetPhaseFrames() const { return phaseFrames_; }
	bool HasCombatTargets() const { return combatTargetsSpawned_; }
	void MarkCombatTargetsSpawned() { combatTargetsSpawned_ = true; }

private:
	void TransitionTo(TutorialPhase next);

	bool enabled_ = false;
	std::unique_ptr<TutorialState> state_;
	int phaseFrames_ = 0;
	bool combatTargetsSpawned_ = false;
};
