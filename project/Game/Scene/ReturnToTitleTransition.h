#pragma once

#include <algorithm>

// タイトル復帰演出のフレーム進行だけを管理する。
// 音声・ポストエフェクト・シーン切替は呼び出し元が担当する。
class ReturnToTitleTransition {
public:
	static constexpr int kDurationFrames = 38;

	void Reset();
	bool Start();
	bool Advance();

	bool IsActive() const { return active_; }
	int GetFrame() const { return frame_; }
	float GetProgress() const {
		return std::clamp(static_cast<float>(frame_) / static_cast<float>(kDurationFrames), 0.0f, 1.0f);
	}

private:
	bool active_ = false;
	int frame_ = 0;
};
