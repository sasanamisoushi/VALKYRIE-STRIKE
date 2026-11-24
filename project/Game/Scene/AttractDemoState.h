#pragma once

#include "engine/math/MyMath.h"

// アトラクトデモの進行、クリア演出の開始状態、無操作飛行の方向を管理する。
// ボス生成、プレイヤー更新、画面描画はGamePlaySceneが担当する。
class AttractDemoState {
public:
	static constexpr int kClearCinematicLeadFrames = 45;

	void Reset(bool playClearCinematic);
	void InitializeFlightDirection(const Vector3& forward);
	void AdvanceFrame();
	void MarkBossCreated() { bossCreated_ = true; }

	bool IsClearCinematic() const { return clearCinematic_; }
	bool IsBossCreated() const { return bossCreated_; }
	bool IsPilotControlLocked() const { return clearCinematic_ && bossCreated_; }
	int GetFrame() const { return frame_; }
	const Vector3& GetFlightDirection() const { return flightDirection_; }
	float GetFlightYaw() const { return flightYaw_; }

private:
	bool clearCinematic_ = false;
	bool bossCreated_ = false;
	int frame_ = 0;
	Vector3 flightDirection_ = { 0.0f, 0.0f, 1.0f };
	float flightYaw_ = 0.0f;
};
