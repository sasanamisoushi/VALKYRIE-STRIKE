#pragma once

#include <list>
#include <memory>

class AttractDemoController;
class Enemy;
class IntroBoostState;
class LockOnManager;
class Obstacle;
class ParticleManager;
class Player;
class TitleLaunchState;
class WarpExitPresentationState;

// ゲームプレイシーンの動作モード。
enum class GameplayRunMode {
	Gameplay,
	AttractDemo,
	TitleBackground,
};

// フレーム内の入力許可フラグ。
struct GameplayInputPermissions {
	bool canUseKeyboardInput = false;
	bool canUseMouseInput = false;
	bool canUsePlayerInput = false;
};

// フレームごとの各要素更新可否を判定するための入力。
struct GameplayFramePolicyInput {
	bool shouldUpdateGame = false;
	bool isBossIntroSequenceActive = false;
	bool isWarpExitActive = false;
	bool isIntroBoostActive = false;
	bool isGameOverActive = false;
	bool canUseKeyboardInput = false;
	bool canUsePlayerInput = false;
};

// フレームごとの各要素更新可否の判定結果。
struct GameplayFrameUpdatePolicy {
	bool isWarpExitSequenceActive = false;
	bool updateSelectedPlayer = false;
	bool updateSelectedMissiles = false;
	bool updateSelectedEnemies = false;
	bool updateSelectedParticles = false;
	bool allowWeaponInput = false;
	bool allowLockOnBehavior = false;
	bool updateAnimationPreview = false;
};

// モード別プレイヤー更新に必要な文脈。
struct GameplayPlayerModeContext {
	GameplayRunMode mode = GameplayRunMode::Gameplay;
	bool updateSelectedPlayer = false;
	bool canUseKeyboardInput = false;
	bool shouldUpdateGame = false;
	bool isEditorPreviewPlaying = false;
	Player* player = nullptr;
	const std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	Enemy* lockedEnemy = nullptr;
	LockOnManager* lockOnManager = nullptr;
	TitleLaunchState* titleLaunchState = nullptr;
	AttractDemoController* attractDemoController = nullptr;
	WarpExitPresentationState* warpExitPresentation = nullptr;
	IntroBoostState* introBoostState = nullptr;
	ParticleManager* particleManager = nullptr;
};

// 通常プレイ・タイトル背景・タイトル出撃・アトラクトデモ・ワープ出口演出ごとの
// プレイヤー更新と操作可否判断を統括する。
class GameplayModeController {
public:
	void Reset() { titleBackgroundFrame_ = 0; }
	int GetTitleBackgroundFrame() const { return titleBackgroundFrame_; }

	GameplayInputPermissions EvaluateInputPermissions(
		GameplayRunMode mode,
		bool isImGuiKeyboardCapture,
		bool isImGuiMouseCapture) const;

	bool ShouldClipMouseCursor(
		GameplayRunMode mode,
		bool isGameOverActive,
		bool isReturnToTitleActive,
		bool shouldUpdateGame,
		bool canUsePlayerInput,
		bool isImGuiVisible) const;

	GameplayFrameUpdatePolicy EvaluateFrameUpdatePolicy(const GameplayFramePolicyInput& input) const;

	void UpdatePlayerForMode(const GameplayPlayerModeContext& context);

private:
	void UpdateTitleLaunchPlayer(Player& player, TitleLaunchState& titleLaunchState) const;
	void UpdateTitleBackgroundAutoFlight(Player& player, const std::list<std::unique_ptr<Obstacle>>& obstacles);

	int titleBackgroundFrame_ = 0;
};
