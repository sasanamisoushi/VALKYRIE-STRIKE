#pragma once

class Camera;
class DamageFeedbackState;
class ParticleManager;
class Player;
class SongFlightTrailRenderer;
class SongState;
class WarpExitPresentationState;
struct GameplayBalanceSettings;

// プレイヤー移動後の飛行・視覚エフェクト更新に必要な文脈。
struct PlayerFlightEffectContext {
	bool shouldUpdateGame = false;
	bool isGameOverActive = false;
	bool isTitleBackgroundMode = false;
	int gameplayElapsedFrames = 0;
	const GameplayBalanceSettings* balanceSettings = nullptr;
	const Player* player = nullptr;
	Camera* activeCamera = nullptr;
	ParticleManager* particleManager = nullptr;
	const SongState* songState = nullptr;
	SongFlightTrailRenderer* songFlightTrailRenderer = nullptr;
	WarpExitPresentationState* warpExitPresentation = nullptr;
};

// 低空高速飛行時のスピードライン、歌発動中の飛行トレイル更新、
// 加速ビネット／被弾ポストエフェクトなど、プレイヤー周辺の視覚演出を統括する。
class PlayerFlightEffectController {
public:
	void UpdatePostEffect(const Player* player, DamageFeedbackState& damageFeedbackState) const;
	void UpdateAfterPlayerMovement(const PlayerFlightEffectContext& context) const;

private:
	void EmitLowAltitudeSpeedRush(const PlayerFlightEffectContext& context) const;
};
