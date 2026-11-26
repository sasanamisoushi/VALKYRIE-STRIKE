#pragma once

#include "2D/Sprite.h"
#include "AttractDemoOverlayRenderer.h"
#include "BossClearOverlayRenderer.h"
#include "BossIntroOverlayRenderer.h"
#include "CombatHudOverlayRenderer.h"
#include "CombatHudSpriteRenderer.h"
#include "GameOverOverlayRenderer.h"
#include "MissionStartOverlayRenderer.h"
#include "PauseMenuOverlayRenderer.h"
#include "RadarRenderer.h"
#include "ReticleOverlayRenderer.h"
#include "ReturnToTitleOverlayRenderer.h"
#include "StageObjectiveOverlayRenderer.h"
#include "TutorialOverlayRenderer.h"

#include <list>
#include <memory>

class AmmoManager;
class AttractDemoState;
class BossClearSequenceController;
class BossEncounterState;
class Camera;
class CombatAlertState;
class Enemy;
class GameOverSequence;
class IntroBoostState;
class LockOnManager;
class MissileManager;
class MultiLockChargeState;
class PauseMenuState;
class Player;
class ReturnToTitleTransition;
class SongState;
class SpecialAttackState;
class TargetSelectionState;
class TutorialGoalMarker;
class TutorialPhaseController;
class TutorialProgressTracker;
class WarpExitPresentationState;
struct TutorialRequirements;

struct GameplayOverlayDrawContext {
	Camera* activeCamera = nullptr;
	bool isTitleBackgroundMode = false;
	bool isAttractDemoMode = false;
	float warpFadeInAlpha = 0.0f;
	int songDurationFrames = 0;
	int gameplayElapsedFrames = 0;
	Player* player = nullptr;
	const std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const AmmoManager* ammoManager = nullptr;
	const MissileManager* missileManager = nullptr;
	LockOnManager* lockOnManager = nullptr;
	const WarpExitPresentationState* warpExitPresentation = nullptr;
	const IntroBoostState* introBoostState = nullptr;
	const ReturnToTitleTransition* returnToTitleTransition = nullptr;
	const BossClearSequenceController* bossClearSequence = nullptr;
	const BossEncounterState* bossEncounterState = nullptr;
	const SongState* songState = nullptr;
	const SpecialAttackState* specialAttackState = nullptr;
	const CombatAlertState* combatAlertState = nullptr;
	const TargetSelectionState* targetSelection = nullptr;
	const MultiLockChargeState* multiLockChargeState = nullptr;
	const TutorialPhaseController* tutorialController = nullptr;
	const TutorialProgressTracker* tutorialProgress = nullptr;
	const TutorialRequirements* tutorialRequirements = nullptr;
	const TutorialGoalMarker* tutorialGoalMarker = nullptr;
	const PauseMenuState* pauseMenuState = nullptr;
	const GameOverSequence* gameOverSequence = nullptr;
	const AttractDemoState* attractDemoState = nullptr;
};

// 戦闘 HUD、レティクル、レーダー、各種演出・ポーズオーバーレイの初期化と描画順を統括する。
class GameplayOverlayCoordinator {
public:
	void Initialize();
	void Draw(const GameplayOverlayDrawContext& context);

private:
	void DrawPauseOverlay(const GameplayOverlayDrawContext& context, float screenWidth, float screenHeight);
	void DrawWarpFadeIn(float warpFadeInAlpha, float screenWidth, float screenHeight);

	std::unique_ptr<Sprite> warpFadeInSprite_;
	std::unique_ptr<Sprite> gameOverWhiteoutSprite_;
	std::unique_ptr<Sprite> bossCinematicTopBarSprite_;
	std::unique_ptr<Sprite> bossCinematicBottomBarSprite_;
	std::unique_ptr<Sprite> missionStartOverlaySprite_;
	std::unique_ptr<Sprite> missionClearOverlaySprite_;
	std::unique_ptr<Sprite> pauseOverlaySprite_;
	std::unique_ptr<Sprite> pausePanelSprite_;
	std::unique_ptr<Sprite> pauseMenuLabelsSprite_;

	MissionStartOverlayRenderer missionStartOverlayRenderer_;
	BossClearOverlayRenderer bossClearOverlayRenderer_;
	CombatHudSpriteRenderer combatHudSpriteRenderer_;
	CombatHudOverlayRenderer combatHudOverlayRenderer_;
	StageObjectiveOverlayRenderer stageObjectiveOverlayRenderer_;
	TutorialOverlayRenderer tutorialOverlayRenderer_;
	RadarRenderer radarRenderer_;
	ReticleOverlayRenderer reticleOverlayRenderer_;
	BossIntroOverlayRenderer bossIntroOverlayRenderer_;
	PauseMenuOverlayRenderer pauseMenuOverlayRenderer_;
	ReturnToTitleOverlayRenderer returnToTitleOverlayRenderer_;
	GameOverOverlayRenderer gameOverOverlayRenderer_;
	AttractDemoOverlayRenderer attractDemoOverlayRenderer_;
};
