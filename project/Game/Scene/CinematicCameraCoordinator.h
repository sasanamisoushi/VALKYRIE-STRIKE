#pragma once

class BossClearCameraState;
class BossClearFocusState;
class BossClearPhaseState;
class BossClearTimelineState;
class Camera;
class GameOverSequence;

// ゲームオーバー演出・ボス撃破演出のシネマティックカメラ更新を統括する。
class CinematicCameraCoordinator {
public:
	// いずれかの演出カメラを適用した場合は true を返す。
	bool TryUpdate(
		Camera* renderCamera,
		bool isDebugCameraActive,
		const GameOverSequence& gameOverSequence,
		const BossClearPhaseState& bossClearPhaseState,
		const BossClearTimelineState& bossClearTimeline,
		const BossClearFocusState& bossClearFocusState,
		BossClearCameraState& bossClearCameraState,
		float phase1DurationSec,
		float phase2FlashDurationSec) const;

private:
	void UpdateGameOverCamera(Camera* renderCamera, const GameOverSequence& gameOverSequence) const;
	void UpdateBossClearCamera(
		Camera* renderCamera,
		const BossClearPhaseState& phaseState,
		const BossClearTimelineState& timeline,
		const BossClearFocusState& focusState,
		BossClearCameraState& cameraState,
		float phase1DurationSec,
		float phase2FlashDurationSec) const;
};
