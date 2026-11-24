#pragma once

// ワープ出口演出の時間進行とゲート表示状態を管理する。
// 位置計算、プレイヤー・カメラ・描画オブジェクトの更新はGamePlaySceneが担当する。
class WarpExitTimelineState {
public:
	static constexpr int kDurationFrames = 210;
	static constexpr int kGateLifetimeFrames = kDurationFrames;
	static constexpr int kPlayerFadeStartFrame = 44;
	static constexpr int kPlayerFadeFrames = 20;
	static constexpr int kGatePassageFrame = 60;
	static constexpr int kGateFadeStartFrame = 76;
	static constexpr int kCameraHandoffStartFrame = 96;
	static constexpr int kCameraHandoffFrames = 52;
	static constexpr int kMissionStartFrame = 120;

	void Reset();
	void Start();
	void AdvanceFrame();
	void Finish();
	bool TryTriggerImpact();
	void HideGate();
	void AdvanceGateFrame();

	bool IsActive() const { return active_; }
	bool IsGateVisible() const { return gateFrames_ > 0; }
	int GetFrame() const { return frame_; }

private:
	bool active_ = false;
	bool impactTriggered_ = false;
	int frame_ = 0;
	int gateFrames_ = 0;
};
