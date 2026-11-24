#pragma once

#include "TutorialRequirementsManager.h"

// チュートリアル中の操作達成状況を保持し、入力を回数・達成状態へ変換する。
// シーンはフェーズ遷移とゲーム世界への副作用だけを担当する。
class TutorialProgressTracker {
public:
	struct FlightStatus {
		bool forwardComplete = false;
		bool altitudeComplete = false;
		bool turnComplete = false;
	};

	struct GerwalkStatus {
		bool hoverComplete = false;
		bool strafeComplete = false;
	};

	void Reset();
	FlightStatus RecordFlight(bool forwardTriggered, bool altitudeInput, bool turnTriggered,
		const TutorialRequirements& requirements);
	GerwalkStatus RecordGerwalk(bool isGerwalk, bool hoverTriggered, bool strafeTriggered,
		const TutorialRequirements& requirements);
	bool RecordLock(bool isLocked, const TutorialRequirements& requirements);
	bool RecordFighterTransform(bool isFighter, const TutorialRequirements& requirements);
	void RecordNormalShot() { ++normalShotsFired_; }
	void RecordHomingLaunch() { ++homingLaunches_; }

	int GetForwardCount() const { return forwardCount_; }
	int GetAltitudeCount() const { return altitudeCount_; }
	int GetTurnCount() const { return turnCount_; }
	int GetGerwalkHoverCount() const { return gerwalkHoverCount_; }
	int GetGerwalkStrafeCount() const { return gerwalkStrafeCount_; }
	int GetLockCount() const { return lockCount_; }
	int GetFighterTransformCount() const { return fighterTransformCount_; }
	int GetNormalShotsFired() const { return normalShotsFired_; }
	int GetHomingLaunches() const { return homingLaunches_; }

private:
	static bool UpdateCount(int& count, bool triggered, int requiredCount);

	int forwardCount_ = 0;
	int altitudeCount_ = 0;
	int turnCount_ = 0;
	int gerwalkHoverCount_ = 0;
	int gerwalkStrafeCount_ = 0;
	int lockCount_ = 0;
	int fighterTransformCount_ = 0;
	int normalShotsFired_ = 0;
	int homingLaunches_ = 0;
	bool altitudeInputActive_ = false;
	bool lockWasActive_ = false;
	bool fighterWasActive_ = false;
};
