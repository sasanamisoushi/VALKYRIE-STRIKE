#pragma once
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "2D/Sprite.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/Particle/ParticleEmitter.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/Audio/AudioManager.h"
#include "engine/Utility/StageLoader.h"
#include "Game/base/BaseScene.h"
#include "Game/Player/Player.h"
#include "3D/Skybox.h"
#include "3D/primitive.h"
#include "3D/Animation.h"
#include "3D/Model.h"
#include "3D/Trail.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/Boss.h"
#include "Game/enemy/JammerEnemy.h"
#include "Game/enemy/GroundEnemy.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/enemy/EnemyEventManager.h"
#include "Game/obstacle/Obstacle.h"
#include "GameCameraManager.h"
#include "CinematicCameraCoordinator.h"
#include "SceneDebugEditorController.h"
#include "StageReloadController.h"
#include "GamePlayUIManager.h"
#include "EnvironmentRenderer.h"
#include "EnemySpawnManager.h"
#include "EnemyUpdateCoordinator.h"
#include "PlayerWeaponController.h"
#include "GameplayModeController.h"
#include "PlayerFlightEffectController.h"
#include "BossEncounterState.h"
#include "BossIntroOverlayRenderer.h"
#include "StageObjectiveOverlayRenderer.h"
#include "MissionStartOverlayRenderer.h"
#include "GameOverOverlayRenderer.h"
#include "PauseMenuOverlayRenderer.h"
#include "ReturnToTitleOverlayRenderer.h"
#include "TutorialOverlayRenderer.h"
#include "BossClearOverlayRenderer.h"
#include "AttractDemoOverlayRenderer.h"
#include "CombatHudOverlayRenderer.h"
#include "CombatHudSpriteRenderer.h"
#include "DebugColliderRenderer.h"
#include "BoundaryShieldRenderer.h"
#include "ReticleOverlayRenderer.h"
#include "RadarRenderer.h"
#include "TutorialRequirementsManager.h"
#include "TutorialProgressTracker.h"
#include "TutorialPhaseController.h"
#include "ReturnToTitleTransition.h"
#include "TutorialGoalMarker.h"
#include "CombatAlertState.h"
#include "DamageFeedbackState.h"
#include "IntroBoostState.h"
#include "GameOverSequence.h"
#include "PauseMenuState.h"
#include "SpecialAttackState.h"
#include "SongState.h"
#include "SongFlightTrailRenderer.h"
#include "AttractDemoController.h"
#include "TitleLaunchState.h"
#include "TitleBackgroundEnemyState.h"
#include "TitleBackgroundEnemyController.h"
#include "TitleBackgroundEnemyFlightController.h"
#include "TitleBackgroundEnemyWeaponController.h"
#include "WarpExitPresentationState.h"
#include "BossClearSequenceController.h"
#include "AmmoManager.h"
#include "BoundaryShieldSettingsManager.h"
#include "GameplayBalanceSettingsManager.h"
#include "MultiLockTarget.h"
#include "MultiLockChargeState.h"
#include "TargetSelectionState.h"
#include "Game/base/GameStartTransition.h"
#include <memory>
#include <vector>
#include <list>
#include <string>
#include <filesystem>
#include <array>
#include <cstdint>
#include <chrono>



class GamePlayScene :public BaseScene {
public:
	friend class GamePlayUIManager;
public:
	enum class Mode {
		Gameplay,
		// Codex のデモ確認用。ゲームプレイのルールとHUDは通常版と同一。
		CodexDemo,
		// リリース版タイトルから起動する、ボス撃破クリア演出のアトラクトデモ。
		AttractDemo,
		// タイトル画面の背面で本編の3Dシーンだけを動かす。操作とHUDは表示しない。
		TitleBackground
	};

	explicit GamePlayScene(Mode mode = Mode::Gameplay);

	// 初期化
	void Initialize() override;

	// 終了
	void Finalize() override;

	// 毎フレーム更新
	void Update() override;

	// 描画
	void Draw() override;

	// タイトル背景の地形を残したまま出撃演出へ移行する。
	void BeginTitleLaunch();
	// ワープ画面で選択を確定した後、タイトル背景用に先読みした本編を操作可能な本編へ切り替える。
	// すでに生成済みのモデル・テクスチャを使うため、Initialize() は呼び直さない。
	void ActivateFromWarp();
	Vector3 GetTitleLaunchForward() const { return titleLaunchState_.GetForward(); }
	Player* GetPlayer() const { return player_.get(); }
	BoundaryShieldSettingsManager& GetBoundaryShieldSettings() { return boundaryShieldSettings_; }
	GameplayBalanceSettingsManager& GetGameplayBalanceSettings() { return gameplayBalanceSettings_; }
	TutorialRequirementsManager& GetTutorialRequirements() { return tutorialRequirements_; }

	// UIの更新

private:
	friend class SimulationManager;


	bool IsTitleBackgroundMode() const { return mode_ == Mode::TitleBackground; }
	bool IsAttractDemoMode() const { return mode_ == Mode::AttractDemo; }
	// タイトル背景だけが周回飛行を使う。
	bool IsAutoFlightMode() const { return IsTitleBackgroundMode(); }
	// ポーズ入力と選択を完結させる。タイトル帰還へ遷移した場合は true を返す。
	bool UpdatePauseMenu(bool canUseKeyboardInput);
	bool IsPauseBlockedByCinematic() const;
	void CancelPauseForCinematic();
	void DrawOverlay();
	// 初回プレイ時に、飛行・変形・武装・標的撃破・離脱地点までを案内する。
	void UpdateTutorial();
	void BeginTutorialCombat();
	void ActivateTutorialGoal();
	void DrawPauseOverlay(float screenWidth, float screenHeight);
	void SetDebugCameraActive(bool isActive);
	void ResetDebugCameraToPlayer();
	void ResetDebugCameraToStageOverview();
	void ReloadSceneJson();
	void ResetEditorPreview();
	MissileTuning MakeMissileTuning(MissileType type) const;
	bool TryConsumeAmmo(MissileType type);
	void UpdateAutoReload();
	// 設定変更に合わせて、ゲームプレイ用BGMとSEをその場で開始・停止する。
	void ApplyAudioSettings();
	GameplayBalanceMissileSettings CollectGameplayBalanceMissileSettings() const;
	void ApplyGameplayBalanceMissileSettings(const GameplayBalanceMissileSettings& missileSettings);
	void CaptureGameplayBalanceDefaults();
	void ApplyGameplayBalanceSettings();
	bool LoadGameplayBalanceSettings();
	bool SaveGameplayBalanceSettings() const;
	void SetCinematicLockOnCameraEnabled(bool enabled);
	bool IsCinematicLockOnCameraEnabled() const;
	void ResetCinematicLockOnCamera();
	// ステージの現在の攻略目標と、ボス出現時の警告カットを HUD 上へ表示する。
	void ClearEnemyReferences(Enemy* enemy);
	void ClearAllEnemyReferences();
	Mode mode_ = Mode::Gameplay;
	AttractDemoController attractDemoController_;

	// シーンリソース
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Sprite> sprite;
	std::unique_ptr<Object3d> groundModel;
	std::unique_ptr<Primitive> myShere;

	ReticleOverlayRenderer reticleOverlayRenderer_;
	std::unique_ptr<Sprite> pauseOverlaySprite_;
	std::unique_ptr<Sprite> pausePanelSprite_;
	std::unique_ptr<Sprite> controlsGuideSprite_;
	std::unique_ptr<Sprite> pauseMenuLabelsSprite_;
	CombatHudSpriteRenderer combatHudSpriteRenderer_;
	RadarRenderer radarRenderer_;
	BoundaryShieldRenderer boundaryShieldRenderer_;
	BoundaryShieldSettingsManager boundaryShieldSettings_;
	static constexpr const char* kBoundaryShieldSettingsPath = "resources/boundary_shield_settings.json";
	std::unique_ptr<Sprite> warpFadeInSprite_;
	std::unique_ptr<Sprite> missionStartOverlaySprite_;
	std::unique_ptr<Sprite> missionClearOverlaySprite_;
	std::unique_ptr<Sprite> gameOverWhiteoutSprite_;
	std::unique_ptr<Sprite> bossCinematicTopBarSprite_;
	std::unique_ptr<Sprite> bossCinematicBottomBarSprite_;
	float warpFadeInAlpha_ = 0.0f;
	WarpExitPresentationState warpExitPresentation_;


	// パーティクル

	// モデル
	std::vector<Object3d *> objects;

	// 音声データ
	SoundData soundData1;
	SoundData soundData2;
	SoundData songSoundData;

	// 再生中のボイスを管理するポインタ
	IXAudio2SourceVoice *pVoice1 = nullptr;
	IXAudio2SourceVoice *pVoice2 = nullptr;
	IXAudio2SourceVoice *pSongVoice = nullptr;

	// プリミティブ
	std::unique_ptr<Primitive> myBox;

	std::unique_ptr<Object3d> myModelObject;

	// 表示切り替えフラグ

	// Partial Ring用パラメータ

	// Cylinder用パラメータ

	// アニメーション用
	Animation animationData;
	float animationTime = 0.0f;
	bool playAnimation = true;

	// スケルトン
	Skeleton skeleton;
	bool showBones = false;
	bool showPlane = false;
	bool showSphere = false;
	bool showBox = false;
	bool showTrail = false;
	bool showSprite = false;
	std::unique_ptr<Model> skeletonLinesModel;
	std::unique_ptr<Object3d> skeletonLinesObject;

	// デバッグ用のコライダー描画
	DebugColliderRenderer debugColliderRenderer_;
	bool showDebugColliders = false;

	// =====================================================
	// デバッグ用フリーカメラ
	// フリーカメラ中は WASD: 移動, 矢印: 回転, Q/E: ロール
	// =====================================================
	std::unique_ptr<FlyCamera> debugFlyCamera_;
	bool isDebugCameraActive_ = false;
	
	SceneDebugEditorController debugEditorController_;

	// 起動直後は編集モードで停止し、ツールバーの「再生」からゲームを開始する。
	bool isEditorPreviewPlaying_ = false;

	// UIと状態管理
	bool showModel = false;
	bool enableSkinning = true; // スキニング（ガワを動かす）の切り替え
	float modelScale = 1.0f;
	int currentAnimationIndex = 0;


	std::unique_ptr<Trail> missileTrail;        // 軌跡の計算を行うクラス
	std::unique_ptr<Object3d> trailObject;      // 軌跡を描画する実体

	float missileNormalSpeed = 1.5f; // 通常弾の速度
	float missileNormalScale = 0.3f;
	float missileNormalCollisionRadius = 0.3f;
	int missileNormalLifeTime = 120;
	float missileNormalHomingStrength = 0.28f;
	float missileSpeed = 0.75f;   // ホーミングミサイルの速度
	float missileAmpX = 15.0f;   // X軸の旋回半径（横の広さ）
	float missileAmpZ = 15.0f;   // Z軸の旋回半径
	float missileAmpY = 3.0f;    // 上下に波打つ高さ
	float missileFreqY = 4.0f;    // 上下に波打つ細かさ（振動数）
	float missileBaseY = 5.0f;    // 基準となる飛行高度
	float missileHomingStrength = 0.085f;
	float missileHomingScale = 0.5f;
	float missileHomingCollisionRadius = 0.5f;
	float missileTrailWidth = 0.5f;
	int missileLifeTime = 240;
	float missileMuzzleOffset = 0.8f;

	GameplayBalanceSettingsManager gameplayBalanceSettings_;
	static constexpr const char* kGameplayBalanceSettingsPath = "resources/gameplay_balance_settings.json";
	static constexpr const char* kTutorialRequirementsPath = "resources/tutorial_requirements.json";

	std::unique_ptr<Player> player_;

	// 画面上に存在するミサイルを管理するマネージャ
	std::unique_ptr<MissileManager> missileManager_;

	// 通常弾で撃破した通常敵の、短時間だけ飛び散る機体パーツ。
	struct NormalEnemyDebrisPiece {
		std::unique_ptr<Object3d> object;
		Vector3 velocity{};
		Vector3 rotation{};
		Vector3 angularVelocity{};
		Vector3 baseScale{};
		int remainingFrames = 0;
	};
	struct HomingEnemyImplosion {
		std::unique_ptr<Object3d> object;
		Vector3 baseScale{};
		Vector3 rotation{};
		int remainingFrames = 0;
	};
	void SpawnNormalEnemyDebris(Enemy& enemy);
	void UpdateNormalEnemyDebris();
	void SpawnHomingEnemyImplosion(Enemy& enemy);
	void UpdateHomingEnemyImplosions();
	void ClearNormalEnemyDebris();
	std::vector<NormalEnemyDebrisPiece> normalEnemyDebris_;
	std::vector<HomingEnemyImplosion> homingEnemyImplosions_;


	// 敵
	std::list<std::unique_ptr<Enemy>> enemies_;
	std::unique_ptr<EnemyBulletManager> enemyBulletManager_;
	std::vector<EnemySpawnData> enemySpawns_;
	EnemyEventManager enemyEventManager_;
	EnemySpawnManager enemySpawnManager_;
	EnemyUpdateCoordinator enemyUpdateCoordinator_;
	TargetSelectionState targetSelection_;
	BossEncounterState bossEncounterState_;
	BossIntroOverlayRenderer bossIntroOverlayRenderer_;
	StageObjectiveOverlayRenderer stageObjectiveOverlayRenderer_;
	MissionStartOverlayRenderer missionStartOverlayRenderer_;
	GameOverOverlayRenderer gameOverOverlayRenderer_;
	PauseMenuOverlayRenderer pauseMenuOverlayRenderer_;
	BossClearSequenceController bossClearSequence_;
	PauseMenuState pauseMenuState_;
	// ポーズメニューからの通信切断・システムシャットダウン遷移演出
	void StartReturnToTitleTransition();
	void UpdateReturnToTitleTransition();
	ReturnToTitleTransition returnToTitleTransition_;
	ReturnToTitleOverlayRenderer returnToTitleOverlayRenderer_;
	TutorialOverlayRenderer tutorialOverlayRenderer_;
	BossClearOverlayRenderer bossClearOverlayRenderer_;
	AttractDemoOverlayRenderer attractDemoOverlayRenderer_;
	CombatHudOverlayRenderer combatHudOverlayRenderer_;


	// 障害物
	std::list<std::unique_ptr<Obstacle>> obstacles_;

	// ImGuiで敵を出すための座標変数
	float newEnemyPos[3] = { 0.0f, 0.0f, 50.0f };

	// 爆発エフェクト
	std::unique_ptr<ExplosionManager> explosionManager_;

	GameOverSequence gameOverSequence_;
	int defeatedEnemyCount_ = 0;
	int gameplayElapsedFrames_ = 0;
	DamageFeedbackState damageFeedbackState_;

	// シミュレーションツールUI用


	// =====================================================
	// マネージャークラス
	// =====================================================
	std::unique_ptr<SimulationManager> simulationManager_;
	std::unique_ptr<MissilePresetManager> missilePresetManager_;
	std::unique_ptr<LockOnManager> lockOnManager_;
	std::unique_ptr<GameCameraManager> cameraManager_;
	CinematicCameraCoordinator cinematicCameraCoordinator_;
	std::unique_ptr<EnvironmentRenderer> environmentRenderer_;
	std::unique_ptr<GamePlayUIManager> uiManager_;
	std::unique_ptr<AmmoManager> ammoManager_;
	IntroBoostState introBoostState_;
	GameplayModeController gameplayModeController_;
	TitleLaunchState titleLaunchState_;

	TitleBackgroundEnemyState titleBackgroundEnemyState_;
	TitleBackgroundEnemyController titleBackgroundEnemyController_;
	TitleBackgroundEnemyFlightController titleBackgroundEnemyFlightController_;
	TitleBackgroundEnemyWeaponController titleBackgroundEnemyWeaponController_;

	StageReloadController stageReloadController_;
	MultiLockChargeState multiLockChargeState_;

	SpecialAttackState specialAttackState_;

	SongState songState_;

	TutorialPhaseController tutorialController_;
	TutorialRequirementsManager tutorialRequirements_;
	TutorialProgressTracker tutorialProgress_;
	TutorialGoalMarker tutorialGoalMarker_;
	// スラスターとは独立した、歌発動中だけの機体移動履歴リボン。
	SongFlightTrailRenderer songFlightTrailRenderer_;
	PlayerFlightEffectController playerFlightEffectController_;

	PlayerWeaponController playerWeaponController_;
	CombatAlertState combatAlertState_;

};
