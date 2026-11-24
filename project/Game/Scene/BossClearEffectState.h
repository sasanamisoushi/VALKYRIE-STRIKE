#pragma once

// ボス撃破演出に付随する短周期エフェクトとクリアUIの状態を管理する。
class BossClearEffectState {
public:
	void Reset();
	void ResetSmokeTimer();
	bool ShouldEmitSmoke(float deltaSeconds, float intervalSeconds);
	// 墜落中だけ使う誘爆用タイマー。黒煙の発生間隔とは独立させる。
	bool ShouldEmitFallingExplosion(float deltaSeconds, float intervalSeconds);
	bool TryInitializePhase3();
	void StartMissionClearUi();
	void AdvanceMissionClearUi(float deltaSeconds);

	bool IsMissionClearUiStarted() const { return missionClearUiStarted_; }
	float GetMissionClearUiSeconds() const { return missionClearUiSeconds_; }

private:
	float smokeSeconds_ = 0.0f;
	float fallingExplosionSeconds_ = 0.0f;
	bool phase3Initialized_ = false;
	bool missionClearUiStarted_ = false;
	float missionClearUiSeconds_ = 0.0f;
};
