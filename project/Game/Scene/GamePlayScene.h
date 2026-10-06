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
#include "LevelManager.h"
#include "GamePlayUIManager.h"
#include "EnvironmentRenderer.h"
#include "AmmoManager.h"
#include "Game/base/GameStartTransition.h"
#include <memory>
#include <vector>
#include <list>
#include <string>
#include <filesystem>
#include <array>



class GamePlayScene :public BaseScene {
public:
	friend class GamePlayUIManager;
public:
	enum class Mode {
		Gameplay,
		Simulation,
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
	Vector3 GetTitleLaunchForward() const { return titleLaunchForward_; }

	// UIの更新

private:
	friend class SimulationManager;
	friend class MissilePresetManager;
	friend class LockOnManager;


	bool IsSimulationMode() const { return mode_ == Mode::Simulation; }
	bool IsTitleBackgroundMode() const { return mode_ == Mode::TitleBackground; }
	void DrawOverlay();
	void DrawRadar();
	void DrawPauseOverlay(float screenWidth, float screenHeight);
	void SetDebugCameraActive(bool isActive);
	void ResetDebugCameraToPlayer();
	void ResetDebugCameraToStageOverview();
	void TryPlaceEnemyAtGameViewMouse();
	void ReloadSceneJson();
	void ResetEditorPreview();
	MissileTuning MakeMissileTuning(MissileType type) const;
	void SpawnEnemyFromSpawnPoint(size_t spawnPointIndex);
	void SpawnDefaultGroundEnemies();
	bool IsEnemySpawnPointActive(size_t spawnPointIndex) const;
	void ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames);
	void TriggerEnemyReinforcements(const std::string &deadEnemyName);
	void UpdateEnemyRespawns();
	bool HasPendingEnemySpawns() const;
	bool TryConsumeAmmo(MissileType type);
	void UpdateAutoReload();
	void UpdateCinematicLockOnCamera();
	void ClearEnemyReferences(Enemy* enemy);
	void ClearAllEnemyReferences();
	Mode mode_ = Mode::Gameplay;

	// シーンリソース
	std::unique_ptr<Camera> camera;
	std::unique_ptr<Sprite> sprite;
	std::unique_ptr<Object3d> groundModel;
	std::unique_ptr<Primitive> myShere;

	std::unique_ptr<Sprite> aimCursorSprite_;
	std::unique_ptr<Sprite> lockOnReticleSprite_;
	std::unique_ptr<Sprite> missileLockOnReticleSprite_;
	static constexpr size_t kMaxMultiLockMarkers = 12;
	std::array<std::unique_ptr<Sprite>, kMaxMultiLockMarkers> multiLockMarkerSprites_;
	std::unique_ptr<Sprite> spGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> spGaugeFillSprite_;
	std::unique_ptr<Sprite> spGaugeCostMarkerSprite_;
	std::unique_ptr<Sprite> songGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> songGaugeFillSprite_;
	std::unique_ptr<Sprite> songGaugeStateSprite_;
	std::unique_ptr<Sprite> hudPanelSprite_;
	std::unique_ptr<Sprite> hudAmmoPanelSprite_;
	std::unique_ptr<Sprite> hpGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> hpGaugeFillSprite_;
	std::unique_ptr<Sprite> bossHpGaugeBackgroundSprite_;
	std::unique_ptr<Sprite> bossHpGaugeFillSprite_;
	std::unique_ptr<Sprite> pauseOverlaySprite_;
	std::unique_ptr<Sprite> pausePanelSprite_;
	std::unique_ptr<Sprite> controlsGuideSprite_;
	std::unique_ptr<Sprite> pauseMenuLabelsSprite_;
	std::unique_ptr<Sprite> hudHpLabelSprite_;
	std::unique_ptr<Sprite> hudAmmoLabelSprite_;
	std::unique_ptr<Sprite> hudSpLabelSprite_;
	std::unique_ptr<Sprite> hudNormalAmmoIconSprite_;
	std::unique_ptr<Sprite> hudHomingAmmoIconSprite_;
	std::unique_ptr<Sprite> hudNormalReloadGaugeSprite_;
	std::unique_ptr<Sprite> hudHomingReloadGaugeSprite_;
	std::unique_ptr<Sprite> radarFrameSprite_;
	std::unique_ptr<Sprite> radarSweepSprite_;
	static constexpr size_t kMaxRadarBlips = 32;
	std::array<std::unique_ptr<Sprite>, kMaxRadarBlips> radarBlipSprites_;
	float radarSweepAngle_ = 0.0f;
	std::array<std::unique_ptr<Sprite>, 3> hudHpDigitSprites_;
	std::array<std::unique_ptr<Sprite>, 7> hudNormalAmmoDigitSprites_;
	std::array<std::unique_ptr<Sprite>, 7> hudHomingAmmoDigitSprites_;
	std::unique_ptr<Object3d> boundaryAlertObject_;
	std::unique_ptr<Object3d> ceilingBoundaryAlertObject_;


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
	std::unique_ptr<Object3d> debugColliderLinesObject;
	bool showDebugColliders = false;

	// =====================================================
	// デバッグ用フリーカメラ
	// フリーカメラ中は WASD: 移動, 矢印: 回転, Q/E: ロール
	// =====================================================
	std::unique_ptr<FlyCamera> debugFlyCamera_;
	bool isDebugCameraActive_ = false;
	
	// ボックス選択用
	bool isBoxSelecting_ = false;
	Vector2 boxSelectStartPos_ = {0.0f, 0.0f};
	Vector2 boxSelectEndPos_ = {0.0f, 0.0f};

	// 起動直後は編集モードで停止し、ツールバーの「再生」からゲームを開始する。
	bool isEditorPreviewPlaying_ = false;
	bool isCinematicLockOnCameraEnabled_ = false;
	bool isCinematicLockOnCameraInitialized_ = false;
	Vector3 cinematicLockOnCameraPosition_ = { 0.0f, 0.0f, 0.0f };
	Quaternion cinematicLockOnCameraRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector3 cinematicLockOnCameraFocus_ = { 0.0f, 0.0f, 0.0f };
	Vector3 cinematicLockOnCameraBackDirection_ = { 0.0f, 0.0f, 1.0f };
	float cinematicLockOnCameraSideSign_ = 1.0f;
	float cinematicLockOnCameraSeparation_ = 0.0f;

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

	std::unique_ptr<Player> player_;

	// 画面上に存在するミサイルを管理するマネージャ
	std::unique_ptr<MissileManager> missileManager_;


	// 敵
	std::list<std::unique_ptr<Enemy>> enemies_;
	std::unique_ptr<EnemyBulletManager> enemyBulletManager_;
	std::vector<EnemySpawnData> enemySpawns_;
	std::vector<int> enemyRespawnTimers_;
	EnemyEventManager enemyEventManager_;
	Enemy *lockedEnemy_ = nullptr;
	bool bossSpawned_ = false;
	bool isPaused_ = false;
	// false: ゲームに戻る / true: タイトルに戻る
	bool isPauseTitleSelected_ = false;


	// 障害物
	std::list<std::unique_ptr<Obstacle>> obstacles_;

	// ImGuiで敵を出すための座標変数
	float newEnemyPos[3] = { 0.0f, 0.0f, 50.0f };

	// 爆発エフェクト
	std::unique_ptr<ExplosionManager> explosionManager_;

	// ゲームオーバー演出用
	bool isGameOver_ = false;
	int gameOverTimer_ = 0;
	int previousPlayerHP_ = -1;
	int damageEffectTimer_ = 0;

	// シミュレーションツールUI用


	// =====================================================
	// マネージャークラス
	// =====================================================
	std::unique_ptr<SimulationManager> simulationManager_;
	std::unique_ptr<MissilePresetManager> missilePresetManager_;
	std::unique_ptr<LockOnManager> lockOnManager_;
	std::unique_ptr<GameCameraManager> cameraManager_;
	std::unique_ptr<LevelManager> levelManager_;
	std::unique_ptr<EnvironmentRenderer> environmentRenderer_;
	std::unique_ptr<GamePlayUIManager> uiManager_;
	std::unique_ptr<AmmoManager> ammoManager_;
	int introBoostFrames_ = 0;
	int introBoostInitialFrames_ = 0;
	float introBoostYaw_ = 0.0f;
	Vector3 introBoostVelocity_ = { 0.0f, 0.0f, 0.0f };
	int titleBackgroundFrame_ = 0;
	bool titleLaunchActive_ = false;
	int titleLaunchFrame_ = 0;
	Vector3 titleLaunchStartPosition_ = { 0.0f, 0.0f, 0.0f };
	Vector3 titleLaunchStartForward_ = { 0.0f, 0.0f, 1.0f };
	Vector3 titleLaunchForward_ = { 0.0f, 0.0f, 1.0f };

	// タイトル背景での敵の再出現・画面外進入演出
	int titleEnemyRespawnTimer_ = 0;
	bool titleEnemyEntering_ = false;
	int titleEnemyEnterFrame_ = 0;
	int titleEnemySpawnSide_ = 1;
	int titleEnemyCombatTimer_ = 0;

	// JSONファイルが最後に更新された日時を記録する変数
	std::filesystem::file_time_type lastJsonWriteTime_;
	Enemy* aimAssistEnemy_ = nullptr;
	std::vector<Enemy*> multiLockTargets_;
	bool isMultiLockCharging_ = false;
	int multiLockChargeFrames_ = 0;
	// シーンを開く操作に使ったクリックを、初弾として扱わない。
	bool isMissileInputArmed_ = false;

	// 必殺技（SPを50%消費して一定時間、通常攻撃を連射）
	float spGauge_ = 100.0f;
	bool isSpecialAttackActive_ = false;
	int specialAttackFrame_ = 0;

	// 歌システム（ルンピカゲージ）
	float songGauge_ = 100.0f;
	bool isSongActive_ = false;
	int songFrame_ = 0;

	// 通常弾の長押し連射用
	int normalFireCooldownTimer_ = 0;

};
