#pragma once

#include <chrono>

// ボス撃破演出の基準時間を管理する。
// フェーズ判定はBossClearPhaseState、粒子ごとの短い間隔はGamePlaySceneが担当する。
class BossClearTimelineState {
public:
	void Reset();
	void Pause();
	float Advance(std::chrono::steady_clock::time_point now);
	void ResetPhase();
	void ResetCinematicFrames();

	float GetSequenceSeconds() const { return sequenceSeconds_; }
	float GetPhaseSeconds() const { return phaseSeconds_; }
	int GetCinematicFrames() const { return cinematicFrames_; }

private:
	std::chrono::steady_clock::time_point lastTickTime_{};
	bool timerInitialized_ = false;
	float sequenceSeconds_ = 0.0f;
	float phaseSeconds_ = 0.0f;
	int cinematicFrames_ = 0;
};
