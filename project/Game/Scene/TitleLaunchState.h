#pragma once

#include "engine/math/MyMath.h"

// タイトル背景から本編へ出撃する演出の開始位置・進行方向・経過フレームを管理する。
// プレイヤー、カメラ、シーンモードの更新はGamePlaySceneが担当する。
class TitleLaunchState {
public:
	static constexpr int kDurationFrames = 84;

	void Reset();
	bool Start(const Vector3& startPosition, const Vector3& forward);
	void AdvanceFrame();

	bool IsActive() const { return active_; }
	int GetFrame() const { return frame_; }
	const Vector3& GetStartPosition() const { return startPosition_; }
	const Vector3& GetForward() const { return forward_; }

private:
	bool active_ = false;
	int frame_ = 0;
	Vector3 startPosition_{};
	Vector3 forward_ = { 0.0f, 0.0f, 1.0f };
};
