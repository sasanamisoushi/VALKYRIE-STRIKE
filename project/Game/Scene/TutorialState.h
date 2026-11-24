#pragma once

#include <memory>

class TutorialProgressTracker;
struct TutorialRequirements;

// チュートリアルの進行状態。各 State は自分の達成条件と次の状態だけを知る。
enum class TutorialPhase {
	Disabled,
	Flight,
	Gerwalk,
	LockOn,
	NormalFire,
	MultiLock,
	Combat,
	Navigation,
	Complete,
};

struct TutorialPhaseInput {
	bool skipRequested = false;
	bool forwardTriggered = false;
	bool altitudeInput = false;
	bool turnTriggered = false;
	bool isGerwalk = false;
	bool gerwalkHoverTriggered = false;
	bool gerwalkStrafeTriggered = false;
	bool isLocked = false;
	bool isFighter = false;
	bool combatTargetsDefeated = false;
	bool goalReached = false;
	bool completeConfirmed = false;
};

struct TutorialPhaseResult {
	bool beginCombat = false;
	bool activateGoal = false;
	bool clearMission = false;
	bool clearGoal = false;
};

class TutorialState {
public:
	virtual ~TutorialState() = default;
	virtual TutorialPhase GetPhase() const = 0;
	virtual TutorialPhase Update(const TutorialPhaseInput& input, const TutorialRequirements& requirements,
		TutorialProgressTracker& progress, TutorialPhaseResult& result) const = 0;
};

std::unique_ptr<TutorialState> CreateTutorialState(TutorialPhase phase);
