#pragma once

#include "engine/math/MyMath.h"

#include <functional>
#include <list>
#include <memory>
#include <vector>

class BossClearSequenceController;
class BossEncounterState;
class Camera;
class CombatAlertState;
class DamageFeedbackState;
class Enemy;
class EnemyBulletManager;
class EnemyDefeatEffectController;
class EnemySpawnManager;
class ExplosionManager;
class GameOverSequence;
class GameplayAudioController;
class LockOnManager;
class MissileManager;
class Obstacle;
class ParticleManager;
class PauseMenuState;
class Player;
class ReturnToTitleTransition;
class SongState;
class TargetSelectionState;
class TutorialGoalMarker;
class TutorialPhaseController;
class TutorialProgressTracker;
struct EnemySpawnData;
struct TutorialRequirements;

struct EditorPreviewResetContext {
	const char* playerModelName = nullptr;
	bool isDebugCameraActive = false;
	bool* isEditorPreviewPlaying = nullptr;
	Camera* camera = nullptr;
	Player* player = nullptr;
	ParticleManager* particleManager = nullptr;
	MissileManager* missileManager = nullptr;
	EnemyBulletManager* enemyBulletManager = nullptr;
	ExplosionManager* explosionManager = nullptr;
	LockOnManager* lockOnManager = nullptr;
	const TargetSelectionState* targetSelection = nullptr;
	const TutorialPhaseController* tutorialController = nullptr;
	GameOverSequence* gameOverSequence = nullptr;
	CombatAlertState* combatAlertState = nullptr;
	DamageFeedbackState* damageFeedbackState = nullptr;
	BossClearSequenceController* bossClearSequence = nullptr;
	EnemyDefeatEffectController* enemyDefeatEffectController = nullptr;
	std::function<void()> clearAllEnemyReferences;
	std::function<void()> reloadSceneJson;
};

struct PauseMenuUpdateContext {
	bool isPauseBlockedByCinematic = false;
	bool canUseKeyboardInput = false;
	bool isSongActive = false;
	PauseMenuState* pauseMenuState = nullptr;
	GameplayAudioController* audioController = nullptr;
	ReturnToTitleTransition* returnToTitleTransition = nullptr;
};

struct TutorialSessionContext {
	bool isBlocked = false;
	int defeatedEnemyCount = 0;
	int gameplayElapsedFrames = 0;
	Player* player = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	EnemySpawnManager* enemySpawnManager = nullptr;
	BossEncounterState* bossEncounterState = nullptr;
	const TargetSelectionState* targetSelection = nullptr;
	TutorialPhaseController* tutorialController = nullptr;
	TutorialProgressTracker* tutorialProgress = nullptr;
	const TutorialRequirements* tutorialRequirements = nullptr;
	TutorialGoalMarker* tutorialGoalMarker = nullptr;
	std::function<void()> clearAllEnemyReferences;
};

class IntroBoostState;
class TitleLaunchState;
class TitleBackgroundEnemyState;
class WarpExitPresentationState;

struct InitialStageTransitionContext {
	bool isAutoFlightMode = false;
	bool isAttractDemoMode = false;
	Player* player = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	EnemySpawnManager* enemySpawnManager = nullptr;
	BossEncounterState* bossEncounterState = nullptr;
	TutorialPhaseController* tutorialController = nullptr;
	TutorialProgressTracker* tutorialProgress = nullptr;
	TutorialGoalMarker* tutorialGoalMarker = nullptr;
	WarpExitPresentationState* warpExitPresentation = nullptr;
	IntroBoostState* introBoostState = nullptr;
	std::function<void()> clearAllEnemyReferences;
};

struct WarpActivationContext {
	Camera* camera = nullptr;
	Player* player = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	TitleLaunchState* titleLaunchState = nullptr;
	TitleBackgroundEnemyState* titleBackgroundEnemyState = nullptr;
	IntroBoostState* introBoostState = nullptr;
	GameOverSequence* gameOverSequence = nullptr;
	CombatAlertState* combatAlertState = nullptr;
	DamageFeedbackState* damageFeedbackState = nullptr;
	BossClearSequenceController* bossClearSequence = nullptr;
	PauseMenuState* pauseMenuState = nullptr;
	TutorialPhaseController* tutorialController = nullptr;
	TutorialProgressTracker* tutorialProgress = nullptr;
	TutorialGoalMarker* tutorialGoalMarker = nullptr;
	int* defeatedEnemyCount = nullptr;
	int* gameplayElapsedFrames = nullptr;
	bool* isEditorPreviewPlaying = nullptr;
	float* warpFadeInAlpha = nullptr;
	std::function<void()> reloadSceneJson;
	std::function<void()> applyAudioSettings;
};

// ポーズ・タイトル帰還遷移・チュートリアル進行・エディタプレビューリセットのセッション制御を担う。
class GameplaySessionFlowController {
public:
	void ApplyInitialStageTransition(const InitialStageTransitionContext& context) const;
	void ActivateFromWarp(const WarpActivationContext& context) const;
	void ResetEditorPreview(const EditorPreviewResetContext& context) const;
	void CancelPauseForCinematic(PauseMenuState& pauseMenuState, bool isSongActive,
		GameplayAudioController& audioController) const;
	// タイトル帰還遷移を開始した場合は true を返す。
	bool UpdatePauseMenu(const PauseMenuUpdateContext& context) const;
	void StartReturnToTitleTransition(ReturnToTitleTransition& transition,
		GameplayAudioController& audioController) const;
	// タイトル画面への遷移が完了したら true を返す。
	bool UpdateReturnToTitleTransition(ReturnToTitleTransition& transition) const;
	// チュートリアル完了により CLEAR シーンへ遷移すべきなら true を返す。
	bool UpdateTutorial(const TutorialSessionContext& context) const;

private:
	void BeginTutorialCombat(const TutorialSessionContext& context) const;
};
