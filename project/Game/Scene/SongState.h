#pragma once

#include <algorithm>

// 歌システムのチャージ量と有効時間を管理する。
// 音声、粒子、弾薬、軌跡オブジェクトの制御はGamePlaySceneが担当する。
class SongState {
public:
	static constexpr float kMaximumGauge = 100.0f;

	void Reset();
	bool CanActivate() const;
	bool TryActivate();
	bool Advance(int durationFrames);
	void AddGauge(float amount);

	float GetGauge() const { return gauge_; }
	int GetFrame() const { return frame_; }
	bool IsActive() const { return active_; }

private:
	float gauge_ = kMaximumGauge;
	bool active_ = false;
	int frame_ = 0;
};
