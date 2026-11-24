#include "TutorialProgressTracker.h"

#include <algorithm>

void TutorialProgressTracker::Reset() {
	forwardCount_ = 0;
	altitudeCount_ = 0;
	turnCount_ = 0;
	gerwalkHoverCount_ = 0;
	gerwalkStrafeCount_ = 0;
	lockCount_ = 0;
	fighterTransformCount_ = 0;
	normalShotsFired_ = 0;
	homingLaunches_ = 0;
	altitudeInputActive_ = false;
	lockWasActive_ = false;
	fighterWasActive_ = false;
}

bool TutorialProgressTracker::UpdateCount(int& count, bool triggered, int requiredCount) {
	requiredCount = (std::max)(requiredCount, 1);
	if (count < requiredCount && triggered) {
		count = (std::min)(count + 1, requiredCount);
	}
	return count >= requiredCount;
}

TutorialProgressTracker::FlightStatus TutorialProgressTracker::RecordFlight(bool forwardTriggered,
	bool altitudeInput, bool turnTriggered, const TutorialRequirements& requirements) {
	const bool altitudeTriggered = altitudeInput && !altitudeInputActive_;
	altitudeInputActive_ = altitudeInput;
	return {
		UpdateCount(forwardCount_, forwardTriggered, requirements.forward),
		UpdateCount(altitudeCount_, altitudeTriggered, requirements.altitude),
		UpdateCount(turnCount_, turnTriggered, requirements.turn),
	};
}

TutorialProgressTracker::GerwalkStatus TutorialProgressTracker::RecordGerwalk(bool isGerwalk,
	bool hoverTriggered, bool strafeTriggered, const TutorialRequirements& requirements) {
	return {
		UpdateCount(gerwalkHoverCount_, isGerwalk && hoverTriggered, requirements.gerwalkHover),
		UpdateCount(gerwalkStrafeCount_, isGerwalk && strafeTriggered, requirements.gerwalkStrafe),
	};
}

bool TutorialProgressTracker::RecordLock(bool isLocked, const TutorialRequirements& requirements) {
	const bool justAcquired = isLocked && !lockWasActive_;
	lockWasActive_ = isLocked;
	return UpdateCount(lockCount_, justAcquired, requirements.lockOn);
}

bool TutorialProgressTracker::RecordFighterTransform(bool isFighter, const TutorialRequirements& requirements) {
	const bool justEngaged = isFighter && !fighterWasActive_;
	fighterWasActive_ = isFighter;
	return UpdateCount(fighterTransformCount_, justEngaged, requirements.fighterTransform);
}
