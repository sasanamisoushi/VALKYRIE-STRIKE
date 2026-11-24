#pragma once

#include "engine/math/MyMath.h"

// ゲームオーバー演出の時間と注視地点だけを管理する。
// カメラ・爆発・音声・画面遷移の実行はGamePlaySceneが担当する。
class GameOverSequence {
public:
	static constexpr int kCinematicFrames = 180;

	void Reset();
	bool Start(const Vector3& focusPosition);
	void Advance();

	bool IsActive() const { return active_; }
	bool HasFinished() const { return frame_ >= kCinematicFrames; }
	int GetFrame() const { return frame_; }
	const Vector3& GetFocusPosition() const { return focusPosition_; }

private:
	bool active_ = false;
	int frame_ = 0;
	Vector3 focusPosition_{};
};
