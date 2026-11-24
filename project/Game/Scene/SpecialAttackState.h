#pragma once

#include <algorithm>

// 必殺技のゲージ消費、継続フレーム、回復を管理する。
// 標的選択・ミサイル発射・プレイヤーの形態変更はGamePlaySceneが担当する。
class SpecialAttackState {
public:
	static constexpr float kMaximumGauge = 100.0f;

	void Reset();
	bool CanStart(float cost) const;
	bool TryStart(float cost);
	bool IsFireFrame(int intervalFrames) const;
	bool Advance(int durationFrames);
	void Recover(float amount);

	float GetGauge() const { return gauge_; }
	bool IsActive() const { return active_; }

private:
	float gauge_ = kMaximumGauge;
	bool active_ = false;
	int frame_ = 0;
};
