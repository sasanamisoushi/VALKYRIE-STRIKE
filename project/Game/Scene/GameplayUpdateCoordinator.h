#pragma once

#include "3D/Animation.h"
#include "3D/Object3d.h"
#include "3D/Primitive.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/math/MyMath.h"

#include <functional>
#include <list>
#include <memory>

class BossClearSequenceController;
class CinematicCameraCoordinator;
class DamageFeedbackState;
class Enemy;
class EnemyDefeatEffectController;
class EnvironmentRenderer;
class ExplosionManager;
class GameCameraManager;
class GameOverSequence;
class GamePlayUIManager;
class GameplayAudioController;
class LockOnManager;
class MissileManager;
class Obstacle;
class ParticleManager;
class Player;
class PlayerFlightEffectController;
class SceneDebugEditorController;
class SimulationManager;
class TargetSelectionState;
class TitleLaunchState;
class TutorialPhaseController;
class WarpExitPresentationState;

struct GameplayGameOverUpdateContext {
	bool isTitleBackgroundMode = false;
	bool isAttractDemoMode = false;
	bool isAttractClearCinematic = false;
	bool isBossClearSequenceActive = false;
	int defeatedEnemyCount = 0;
	int* gameplayElapsedFrames = nullptr;
	Player* player = nullptr;
	ExplosionManager* explosionManager = nullptr;
	ParticleManager* particleManager = nullptr;
	GameplayAudioController* audioController = nullptr;
	GameOverSequence* gameOverSequence = nullptr;
	const TutorialPhaseController* tutorialController = nullptr;
	PlayerFlightEffectController* playerFlightEffectController = nullptr;
	DamageFeedbackState* damageFeedbackState = nullptr;
	std::function<void()> cancelPauseForCinematic;
};

struct GameplayDebugAnimationContext {
	bool playAnimation = true;
	bool updateAnimationPreview = false;
	bool enableSkinning = true;
	bool showBones = false;
	bool showModel = false;
	float modelScale = 1.0f;
	float* animationTime = nullptr;
	Primitive* myBox = nullptr;
	Object3d* myModelObject = nullptr;
	Object3d* skeletonLinesObject = nullptr;
	Animation* animationData = nullptr;
	Skeleton* skeleton = nullptr;
	const Player* player = nullptr;
	const SimulationManager* simulationManager = nullptr;
};

struct GameplayCameraUpdateContext {
	bool isDebugCameraActive = false;
	bool canUseKeyboardInput = false;
	bool isAutoFlightMode = false;
	int titleBackgroundFrame = 0;
	Camera* camera = nullptr;
	FlyCamera* debugFlyCamera = nullptr;
	Player* player = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	CinematicCameraCoordinator* cinematicCameraCoordinator = nullptr;
	const GameOverSequence* gameOverSequence = nullptr;
	BossClearSequenceController* bossClearSequence = nullptr;
	const TitleLaunchState* titleLaunchState = nullptr;
	const TargetSelectionState* targetSelection = nullptr;
	const LockOnManager* lockOnManager = nullptr;
	WarpExitPresentationState* warpExitPresentation = nullptr;
	GameCameraManager* cameraManager = nullptr;
	SceneDebugEditorController* debugEditorController = nullptr;
	GamePlayUIManager* uiManager = nullptr;
	SimulationManager* simulationManager = nullptr;
};

struct GameplayMissileUpdateContext {
	bool updateSelectedMissiles = false;
	bool shouldUpdateGame = false;
	bool isBossClearSequenceActive = false;
	bool updateSelectedParticles = false;
	bool isDestructionEffectPreviewActive = false;
	Camera* activeCamera = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	MissileManager* missileManager = nullptr;
	ExplosionManager* explosionManager = nullptr;
	ParticleManager* particleManager = nullptr;
	EnemyDefeatEffectController* enemyDefeatEffectController = nullptr;
};

class BossEncounterState;
class CombatAlertState;
class EnemyBulletManager;
class EnemyEventManager;
class EnemySpawnManager;
class EnemyUpdateCoordinator;
class MissilePresetManager;
class SongState;
class TitleBackgroundEnemyController;
class TitleBackgroundEnemyFlightController;
class TitleBackgroundEnemyState;
class TitleBackgroundEnemyWeaponController;
struct EnemySpawnData;

struct GameplayEnemyPhaseContext {
	bool updateSelectedEnemies = false;
	bool isAutoFlightMode = false;
	bool isGameOverActive = false;
	int bossMaxActiveMinions = 0;
	int titleBackgroundFrame = 0;
	Camera* camera = nullptr;
	Player* player = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	EnemyBulletManager* enemyBulletManager = nullptr;
	ExplosionManager* explosionManager = nullptr;
	ParticleManager* particleManager = nullptr;
	CombatAlertState* combatAlertState = nullptr;
	EnemyEventManager* enemyEventManager = nullptr;
	EnemySpawnManager* enemySpawnManager = nullptr;
	EnemyUpdateCoordinator* enemyUpdateCoordinator = nullptr;
	EnemyDefeatEffectController* enemyDefeatEffectController = nullptr;
	SongState* songState = nullptr;
	BossClearSequenceController* bossClearSequence = nullptr;
	BossEncounterState* bossEncounterState = nullptr;
	TargetSelectionState* targetSelection = nullptr;
	TitleBackgroundEnemyState* titleBackgroundEnemyState = nullptr;
	TitleBackgroundEnemyController* titleBackgroundEnemyController = nullptr;
	TitleBackgroundEnemyFlightController* titleBackgroundEnemyFlightController = nullptr;
	TitleBackgroundEnemyWeaponController* titleBackgroundEnemyWeaponController = nullptr;
	MissilePresetManager* missilePresetManager = nullptr;
	const GamePlayUIManager* uiManager = nullptr;
	int* defeatedEnemyCount = nullptr;
	std::function<void(Enemy*)> clearEnemyReferences;
};

// GamePlayScene::Update() 内の各更新フェーズ（ゲームオーバー・ボーンプレビュー・敵/ウェーブ・カメラ・弾/撃破演出）を統括する。
class GameplayUpdateCoordinator {
public:
	// 敵更新・撃破処理・タイトル背景敵・ウェーブ/ボス進行を処理する。CLEAR遷移を要求したら true を返す。
	bool UpdateEnemyAndWavePhase(const GameplayEnemyPhaseContext& context) const;
	// ゲームオーバー開始・スロー演出・遷移を処理する。GAMEOVER遷移を要求したら true を返す。
	bool UpdateGameOverPhase(const GameplayGameOverUpdateContext& context, bool& shouldUpdateGame) const;

	// デバッグ用アニメーションキューブとボーン線分モデルを更新する。
	void UpdateDebugAnimationAndBones(const GameplayDebugAnimationContext& context) const;

	// デバッグカメラ／演出カメラ／通常追従カメラを更新し、障害物の WVP を同期する。
	void UpdateSceneCamera(const GameplayCameraUpdateContext& context) const;

	// ミサイル更新・ヒット/シールド/撃破演出・爆発エフェクト更新を実行する。
	void UpdateMissilesAndHitEffects(const GameplayMissileUpdateContext& context) const;
};
