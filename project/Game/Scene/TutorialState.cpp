#include "TutorialState.h"

#include "TutorialProgressTracker.h"
#include "TutorialRequirementsManager.h"

namespace {
	class DisabledTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::Disabled; }
		TutorialPhase Update(const TutorialPhaseInput&, const TutorialRequirements&, TutorialProgressTracker&,
			TutorialPhaseResult&) const override {
			return TutorialPhase::Disabled;
		}
	};

	class FlightTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::Flight; }
		TutorialPhase Update(const TutorialPhaseInput& input, const TutorialRequirements& requirements,
			TutorialProgressTracker& progress, TutorialPhaseResult&) const override {
			const auto status = progress.RecordFlight(input.forwardTriggered, input.altitudeInput, input.turnTriggered, requirements);
			return status.forwardComplete && status.altitudeComplete && status.turnComplete
				? TutorialPhase::Gerwalk : TutorialPhase::Flight;
		}
	};

	class GerwalkTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::Gerwalk; }
		TutorialPhase Update(const TutorialPhaseInput& input, const TutorialRequirements& requirements,
			TutorialProgressTracker& progress, TutorialPhaseResult& result) const override {
			const auto status = progress.RecordGerwalk(input.isGerwalk, input.gerwalkHoverTriggered,
				input.gerwalkStrafeTriggered, requirements);
			if (!input.isGerwalk || !status.hoverComplete || !status.strafeComplete) {
				return TutorialPhase::Gerwalk;
			}
			result.beginCombat = true;
			return TutorialPhase::LockOn;
		}
	};

	class LockOnTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::LockOn; }
		TutorialPhase Update(const TutorialPhaseInput& input, const TutorialRequirements& requirements,
			TutorialProgressTracker& progress, TutorialPhaseResult&) const override {
			return progress.RecordLock(input.isLocked, requirements) ? TutorialPhase::NormalFire : TutorialPhase::LockOn;
		}
	};

	class NormalFireTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::NormalFire; }
		TutorialPhase Update(const TutorialPhaseInput&, const TutorialRequirements& requirements,
			TutorialProgressTracker& progress, TutorialPhaseResult&) const override {
			return progress.GetNormalShotsFired() >= requirements.normalShots
				? TutorialPhase::MultiLock : TutorialPhase::NormalFire;
		}
	};

	class MultiLockTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::MultiLock; }
		TutorialPhase Update(const TutorialPhaseInput& input, const TutorialRequirements& requirements,
			TutorialProgressTracker& progress, TutorialPhaseResult&) const override {
			return progress.RecordFighterTransform(input.isFighter, requirements) &&
				progress.GetHomingLaunches() >= requirements.homingLaunches
				? TutorialPhase::Combat : TutorialPhase::MultiLock;
		}
	};

	class CombatTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::Combat; }
		TutorialPhase Update(const TutorialPhaseInput& input, const TutorialRequirements&, TutorialProgressTracker&,
			TutorialPhaseResult& result) const override {
			if (!input.combatTargetsDefeated) {
				return TutorialPhase::Combat;
			}
			result.activateGoal = true;
			return TutorialPhase::Navigation;
		}
	};

	class NavigationTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::Navigation; }
		TutorialPhase Update(const TutorialPhaseInput& input, const TutorialRequirements&, TutorialProgressTracker&,
			TutorialPhaseResult& result) const override {
			if (!input.goalReached) {
				return TutorialPhase::Navigation;
			}
			result.clearGoal = true;
			result.clearMission = true;
			return TutorialPhase::Disabled;
		}
	};

	class CompleteTutorialState final : public TutorialState {
	public:
		TutorialPhase GetPhase() const override { return TutorialPhase::Complete; }
		TutorialPhase Update(const TutorialPhaseInput& input, const TutorialRequirements&, TutorialProgressTracker&,
			TutorialPhaseResult&) const override {
			return input.completeConfirmed ? TutorialPhase::Disabled : TutorialPhase::Complete;
		}
	};
}

std::unique_ptr<TutorialState> CreateTutorialState(TutorialPhase phase) {
	switch (phase) {
	case TutorialPhase::Flight: return std::make_unique<FlightTutorialState>();
	case TutorialPhase::Gerwalk: return std::make_unique<GerwalkTutorialState>();
	case TutorialPhase::LockOn: return std::make_unique<LockOnTutorialState>();
	case TutorialPhase::NormalFire: return std::make_unique<NormalFireTutorialState>();
	case TutorialPhase::MultiLock: return std::make_unique<MultiLockTutorialState>();
	case TutorialPhase::Combat: return std::make_unique<CombatTutorialState>();
	case TutorialPhase::Navigation: return std::make_unique<NavigationTutorialState>();
	case TutorialPhase::Complete: return std::make_unique<CompleteTutorialState>();
	case TutorialPhase::Disabled: return std::make_unique<DisabledTutorialState>();
	}
	return std::make_unique<DisabledTutorialState>();
}
