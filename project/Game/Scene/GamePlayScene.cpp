#include "GamePlayScene.h"
#include "Game/Scene/ResultData.h"
#include "SimulationManager.h"
#include "MissilePresetManager.h"
#include "LockOnManager.h"
#include "GamePlaySceneHelpers.h"
#include "3D/ModelManager.h"
#include <Windows.h>
#include "engine/Graphics/DirectXCommon.h"
#include "2D/SpriteCommon.h"
#include "3D/Object3dCommon.h"
#include "engine/Input/Input.h"
#include "engine/Debug/ImGuiManager.h"
#include "engine/Resource/TextureManager.h"
#include <externals/imgui/imgui.h>
#include "engine/Camera/FlyCamera.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Scene/SceneManager.h"
#include "engine/Utility/StageLoader.h"
#include "engine/Utility/StageValidation.h"
#include "externals/json.hpp"
#include "Game/editor/EditorReceiver.h"
#include "Game/enemy/Boss.h"
#include "Game/base/GameSettings.h"
#include "Game/Scene/TitleScene.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <shellapi.h>

using json = nlohmann::json;

namespace {
	constexpr char kSongParticleGroup[] = "songSparkle";
	constexpr char kSpeedRushParticleGroup[] = "lowAltitudeSpeedRush";

	float SmoothStep01(float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}

	Vector3 CalculateTitleLaunchCameraShake(float progress, int frame) {
		// 機体が加速するほど、エンジンの振動が少しずつカメラへ伝わる。
		const float acceleration = SmoothStep01((progress - 0.20f) / 0.54f);
		const float engineAmplitude = 0.025f + acceleration * 0.115f;

		// フォールドが開く瞬間だけは、短く強い衝撃を追加する。
		const float impactProgress = std::clamp(1.0f - std::abs(progress - 0.77f) / 0.15f, 0.0f, 1.0f);
		const float impactAmplitude = impactProgress * impactProgress * 0.36f;
		const float time = static_cast<float>(frame);

		return {
			std::sin(time * 2.17f) * engineAmplitude + std::sin(time * 5.71f) * impactAmplitude,
			std::cos(time * 2.83f + 0.7f) * engineAmplitude * 0.62f + std::cos(time * 7.13f) * impactAmplitude * 0.58f,
			std::sin(time * 3.41f + 1.4f) * engineAmplitude * 0.30f - impactAmplitude * 0.22f,
		};
	}
}




GamePlayScene::GamePlayScene(Mode mode)
	: mode_(mode) {
}

void GamePlayScene::SetCinematicLockOnCameraEnabled(bool enabled) {
	if (cameraManager_) {
		cameraManager_->SetCinematicLockOnEnabled(enabled);
	}
}

bool GamePlayScene::IsCinematicLockOnCameraEnabled() const {
	return cameraManager_ && cameraManager_->IsCinematicLockOnEnabled();
}

void GamePlayScene::ResetCinematicLockOnCamera() {
	if (cameraManager_) {
		cameraManager_->ResetCinematicCamera();
	}
}

GameplayBalanceMissileSettings GamePlayScene::CollectGameplayBalanceMissileSettings() const {
	GameplayBalanceMissileSettings missileSettings{};
	missileSettings.normalSpeed = missileNormalSpeed;
	missileSettings.normalHomingStrength = missileNormalHomingStrength;
	missileSettings.normalScale = missileNormalScale;
	missileSettings.normalCollisionRadius = missileNormalCollisionRadius;
	missileSettings.normalLifeTime = missileNormalLifeTime;
	missileSettings.homingSpeed = missileSpeed;
	missileSettings.homingStrength = missileHomingStrength;
	missileSettings.homingScale = missileHomingScale;
	missileSettings.homingCollisionRadius = missileHomingCollisionRadius;
	missileSettings.homingTrailWidth = missileTrailWidth;
	missileSettings.homingLifeTime = missileLifeTime;
	missileSettings.muzzleOffset = missileMuzzleOffset;
	return missileSettings;
}

void GamePlayScene::ApplyGameplayBalanceMissileSettings(const GameplayBalanceMissileSettings& missileSettings) {
	missileNormalSpeed = missileSettings.normalSpeed;
	missileNormalHomingStrength = missileSettings.normalHomingStrength;
	missileNormalScale = missileSettings.normalScale;
	missileNormalCollisionRadius = missileSettings.normalCollisionRadius;
	missileNormalLifeTime = missileSettings.normalLifeTime;
	missileSpeed = missileSettings.homingSpeed;
	missileHomingStrength = missileSettings.homingStrength;
	missileHomingScale = missileSettings.homingScale;
	missileHomingCollisionRadius = missileSettings.homingCollisionRadius;
	missileTrailWidth = missileSettings.homingTrailWidth;
	missileLifeTime = missileSettings.homingLifeTime;
	missileMuzzleOffset = missileSettings.muzzleOffset;
}

void GamePlayScene::CaptureGameplayBalanceDefaults() {
	gameplayBalanceSettings_.CaptureDefaults(player_.get());
}

void GamePlayScene::ApplyGameplayBalanceSettings() {
	GameplayBalanceMissileSettings missileSettings = CollectGameplayBalanceMissileSettings();
	gameplayBalanceSettings_.Apply(player_.get(), &missileSettings);
	ApplyGameplayBalanceMissileSettings(missileSettings);
}

bool GamePlayScene::LoadGameplayBalanceSettings() {
	GameplayBalanceMissileSettings missileSettings = CollectGameplayBalanceMissileSettings();
	if (!gameplayBalanceSettings_.Load(kGameplayBalanceSettingsPath, player_.get(), &missileSettings)) {
		return false;
	}
	ApplyGameplayBalanceMissileSettings(missileSettings);
	return true;
}

bool GamePlayScene::SaveGameplayBalanceSettings() const {
	const GameplayBalanceMissileSettings missileSettings = CollectGameplayBalanceMissileSettings();
	return gameplayBalanceSettings_.Save(kGameplayBalanceSettingsPath, &missileSettings);
}

void GamePlayScene::BeginTitleLaunch() {
	if (!IsTitleBackgroundMode() || !player_ || titleLaunchState_.IsActive()) {
		return;
	}

	titleLaunchState_.Start(player_->GetPosition(), player_->GetForwardVector());
}

void GamePlayScene::ActivateFromWarp() {
	// 1. 本編ゲームプレイモードへ確実に切り替え（PDF原因2対策）
	mode_ = Mode::Gameplay;
	titleLaunchState_.Reset();
	titleBackgroundEnemyState_.Reset();

	// 2. 本編用カメラへの復帰と初期化
	if (camera) {
		Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
	}

	// 3. 地形・敵データの再読み込みとコライダー復帰（PDF原因1対策）
	ReloadSceneJson();
	for (auto& obstacle : obstacles_) {
		if (obstacle) {
			obstacle->SetCollisionEnabled(true);
			obstacle->Update();
		}
	}

	// 4. 本編音声の開始（保存済みの音声設定をそのまま反映する）
	ApplyAudioSettings();

	// 5. プレイヤーの移動制御フラグと推力・コライダー復帰（PDF原因1・2対策）
	if (player_) {
		player_->SetSpecialAttackActive(false);
		player_->ChangeMode(PlayerMode::Fighter);

		// ファイター形態の前進基本推力を直ちに付与し、前進停止を防止
		const Vector3 forward = player_->GetForwardVector();
		const float baseSpeed = player_->GetModeParams(PlayerMode::Fighter).maxMoveSpeed * 0.8f;
		player_->SetVelocity({ forward.x * baseSpeed, forward.y * baseSpeed, forward.z * baseSpeed });
	}

	// 6. 出撃ブーストフラグの初期化（入力をロックさせない）
	GameStartTransitionData titleTransition;
	GameStartTransition::Consume(titleTransition);
	introBoostState_.Reset();

	// 7. ゲーム進行フラグ・物理演算・タイムスケール完全復帰（PDF原因3対策）
	gameOverSequence_.Reset();
	combatAlertState_.Reset();
	damageFeedbackState_.Reset();
	bossClearSequence_.Reset();
	defeatedEnemyCount_ = 0;
	gameplayElapsedFrames_ = 0;
	pauseMenuState_.Reset();
	isEditorPreviewPlaying_ = true; // shouldUpdateGame を確実に有効化
	warpFadeInAlpha_ = 1.0f;
	tutorialController_.Reset(tutorialController_.IsEnabled());
	tutorialProgress_.Reset();
	tutorialGoalMarker_.Reset();
}

bool GamePlayScene::TryConsumeAmmo(MissileType type) {
	// 歌が有効な間は、全武器の弾倉を消費せず発射を許可する。
	// 弾倉を毎フレーム補充する方式ではリロード中の武器が撃てないため、
	// 消費処理そのものを通さないことで本当の無限弾にする。
	if (songState_.IsActive()) {
		return true;
	}
	if (!ammoManager_) {
		return false;
	}
	if (ammoManager_->TryConsume(type)) {
		return true;
	}

	const bool isNormal = type == MissileType::Normal;
	const bool isReloading = isNormal ? ammoManager_->IsNormalReloading() : ammoManager_->IsHomingReloading();
	if (isReloading) {
		// 複数ロックで複数回呼ばれても、表示時間を同じ長さへ戻すだけにする。
		combatAlertState_.TriggerReloadWarning(!isNormal);
	}
	return false;
}

void GamePlayScene::UpdateAutoReload() {
	if (ammoManager_) ammoManager_->UpdateAutoReload();
}

void GamePlayScene::ClearEnemyReferences(Enemy* enemy) {
	if (!enemy) {
		return;
	}

	if (targetSelection_.GetLockedEnemy() == enemy) {
		ResetCinematicLockOnCamera();
	}
	targetSelection_.ClearEnemy(enemy);
	multiLockChargeState_.RemoveTargetsByEnemy(enemy);
	if (missileManager_) {
		missileManager_->ClearTarget(enemy);
	}
}

void GamePlayScene::ClearAllEnemyReferences() {
	targetSelection_.ClearAll();
	multiLockChargeState_.ClearTargets();
	ResetCinematicLockOnCamera();
	if (missileManager_) {
		missileManager_->ClearAllTargets();
	}
}

void GamePlayScene::Initialize() {

	boundaryShieldSettings_.Load(kBoundaryShieldSettingsPath, nullptr);
	tutorialRequirements_.Load(kTutorialRequirementsPath);

	camera = std::make_unique<Camera>();
	ammoManager_ = std::make_unique<AmmoManager>();
	uiManager_ = std::make_unique<GamePlayUIManager>(this);
	environmentRenderer_ = std::make_unique<EnvironmentRenderer>();
	environmentRenderer_->Initialize();
	camera->SetRotate({ 0.0f,0.0f,0.0f });
	camera->SetTranslate({ 0.0f,0.0f,-10.0f });
	Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());


	sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance() , "resources/uvChecker.png");
	reticleOverlayRenderer_.Initialize();

	TextureManager::GetInstance()->LoadTexture("resources/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("resources/mission_start_overlay.png");
	TextureManager::GetInstance()->LoadTexture("resources/mission_clear_overlay.png");
	warpFadeInSprite_ = std::make_unique<Sprite>();
	warpFadeInSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	warpFadeInSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	gameOverWhiteoutSprite_ = std::make_unique<Sprite>();
	gameOverWhiteoutSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	gameOverWhiteoutSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	bossCinematicTopBarSprite_ = std::make_unique<Sprite>();
	bossCinematicTopBarSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	bossCinematicTopBarSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	bossCinematicBottomBarSprite_ = std::make_unique<Sprite>();
	bossCinematicBottomBarSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	bossCinematicBottomBarSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	missionStartOverlaySprite_ = std::make_unique<Sprite>();
	missionStartOverlaySprite_->Initialize(SpriteCommon::GetInstance(), "resources/mission_start_overlay.png");
	missionStartOverlaySprite_->SetAnchorPoint({ 0.5f, 0.5f });
	missionClearOverlaySprite_ = std::make_unique<Sprite>();
	missionClearOverlaySprite_->Initialize(SpriteCommon::GetInstance(), "resources/mission_clear_overlay.png");
	missionClearOverlaySprite_->SetAnchorPoint({ 0.5f, 0.5f });
	warpFadeInAlpha_ = 1.0f;
	TextureManager::GetInstance()->LoadTexture("resources/gameplay_controls.png");
	TextureManager::GetInstance()->LoadTexture("resources/pause_menu_labels.png");
	pauseOverlaySprite_ = std::make_unique<Sprite>();
	pauseOverlaySprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	pausePanelSprite_ = std::make_unique<Sprite>();
	pausePanelSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	controlsGuideSprite_ = std::make_unique<Sprite>();
	controlsGuideSprite_->Initialize(SpriteCommon::GetInstance(), "resources/gameplay_controls.png");
	pauseMenuLabelsSprite_ = std::make_unique<Sprite>();
	pauseMenuLabelsSprite_->Initialize(SpriteCommon::GetInstance(), "resources/pause_menu_labels.png");
	combatHudSpriteRenderer_.Initialize();
	radarRenderer_.Initialize();
	specialAttackState_.Reset();
	songState_.Reset();
	playerWeaponController_.Reset();
	tutorialController_.Reset(false);
	tutorialProgress_.Reset();
	ammoManager_->Initialize();

	boundaryShieldRenderer_.Initialize();


	// SkyboxCommon is now initialized in Framework.cpp





	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");
	ModelManager::GetInstance()->CreateSphereModel("Sphere", 16);
	ModelManager::GetInstance()->CreateSphereModel("EnemyBulletSphere", 6);
	// 地上敵の射撃用。球を進行方向へ細長く伸ばすことで、箱型ではない丸いビーム芯になる。
	ModelManager::GetInstance()->CreateSphereModel("GroundBeamCore", 12);
	if (Model *model = ModelManager::GetInstance()->FindModel("GroundBeamCore")) {
		model->SetColor({ 0.10f, 0.90f, 1.0f, 1.0f });
		model->SetEnableLighting(false);
	}
	// ボス主砲は地上敵のビームと同じ丸い芯・残光を共有しつつ、射撃時だけ大きく伸ばす。
	ModelManager::GetInstance()->CreateSphereModel("BossGroundBeam", 12);
	if (Model *model = ModelManager::GetInstance()->FindModel("BossGroundBeam")) {
		model->SetColor({ 0.28f, 0.96f, 1.0f, 1.0f });
		model->SetEnableLighting(false);
	}

	//======================================================

	//======================================================


	// groundModel = std::make_unique<Object3d>();
	// groundModel->Initialize(Object3dCommon::GetInstance());
	// groundModel->SetModel("plane.obj");
	// groundModel->SetScale({ 3000.0f, 1.0f, 3000.0f });
	// groundModel->SetTranslate({ 0.0f, 0.0f, 0.0f });
	// objects.push_back(groundModel.get());


	myShere = std::make_unique<Primitive>();
	myShere->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Sphere);
	myShere->SetTranslate({ 2.0f,0.0f,0.0f });
	// objects.push_back(myShere.get());

	if (myShere->GetModel()) {
		myShere->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
	}


	myBox = std::make_unique<Primitive>();
	myBox->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Box);
	myBox->SetTranslate({ -2.0f,0.0f,0.0f });



	myModelObject = std::make_unique<Object3d>();
	myModelObject->Initialize(Object3dCommon::GetInstance());
	ModelManager::GetInstance()->LoadModel("AnimatedCube/AnimatedCube.gltf");
	myModelObject->SetModel("AnimatedCube/AnimatedCube.gltf");
	//objects.push_back(myModelObject.get());


	animationData = LoadAnimationFile("resources/AnimatedCube", "AnimatedCube.gltf");
	Node rootNode = Model::LoadNodeHierarchy("resources/AnimatedCube", "AnimatedCube.gltf");
	skeleton = CreateSkeleton(rootNode);
	if (!skeleton.joints.empty()) {
		skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
	}

	myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);


	ModelManager::GetInstance()->CreateLineModel("SkeletonLines");
	skeletonLinesObject = std::make_unique<Object3d>();
	skeletonLinesObject->Initialize(Object3dCommon::GetInstance());
	skeletonLinesObject->SetModel("SkeletonLines");


	debugColliderRenderer_.Initialize();


	debugFlyCamera_ = std::make_unique<FlyCamera>();
	debugFlyCamera_->SetTranslate({ 0.0f, 5.0f, -20.0f });
	isDebugCameraActive_ = false;









	environmentRenderer_->GetParticleManager()->CreateParticleGroup("test", "resources/circle.png");
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("smoke", "resources/circle.png");
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("blackSmoke", "resources/circle.png");
	environmentRenderer_->GetParticleManager()->SetParticleGroupAlphaBlend("blackSmoke", true);
	// 歌の最中だけプレイヤーの後方へ流す、黄色いスラスター状の残光。
	environmentRenderer_->GetParticleManager()->CreateParticleGroup(kSongParticleGroup, "resources/circle.png");
	// 低空高速時にカメラ近傍を横切る、細い大気・ダストの流れ。
	environmentRenderer_->GetParticleManager()->CreateParticleGroup(kSpeedRushParticleGroup, "resources/warp_speedline_particle.png");
	// 地面すれすれの飛行時にだけ、機体真下から舞い上がる土埃（ジェット風圧・巨大砂煙トレイル・砂礫デブリ）。
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("lowAltitudeDownwash", "resources/low_altitude_dust_puff.png");
	environmentRenderer_->GetParticleManager()->SetParticleGroupAlphaBlend("lowAltitudeDownwash", true);
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("lowAltitudeDustTrail", "resources/low_altitude_dust_puff.png");
	environmentRenderer_->GetParticleManager()->SetParticleGroupAlphaBlend("lowAltitudeDustTrail", true);
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("lowAltitudeDustDebris", "resources/white1x1.png");
	environmentRenderer_->GetParticleManager()->SetParticleGroupAlphaBlend("lowAltitudeDustDebris", true);


	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");
	songSoundData = AudioManager::GetInstance()->LoadAudio("resources/song_bgm.mp3");

	// タイトル背景では無音。本編では保存済み設定に応じて再生する。
	AudioManager::GetInstance()->SetMasterVolume(GameSettings::GetInstance().GetMasterVolume());
	ApplyAudioSettings();


	ModelManager::GetInstance()->CreateTrailModel("SmokeTrail");


	missileTrail = std::make_unique<Trail>();
	missileTrail->Initialize(60);


	trailObject = std::make_unique<Object3d>();
	trailObject->Initialize(Object3dCommon::GetInstance());
	trailObject->SetModel("SmokeTrail");

	// 歌の残像はノズルの炎と共有せず、胴体と両翼端の飛行履歴を別々に保持する。
	songFlightTrailRenderer_.Initialize();

	bossClearSequence_.Initialize();


	ModelManager::GetInstance()->CreateBoxModel("PlayerBox");
	ModelManager::GetInstance()->CreateBoxModel("EnemyBox");
	ModelManager::GetInstance()->CreateBoxModel("BossHull");
	ModelManager::GetInstance()->CreateBoxModel("BossCannon");
	ModelManager::GetInstance()->CreateBoxModel("BossBeam");
	ModelManager::GetInstance()->CreateBoxModel("NormalEnemyDebris");
	if (Model *model = ModelManager::GetInstance()->FindModel("BossHull")) model->SetColor({ 0.18f, 0.22f, 0.35f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("BossCannon")) model->SetColor({ 1.0f, 0.35f, 0.05f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("BossBeam")) model->SetColor({ 0.15f, 0.8f, 1.0f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("NormalEnemyDebris")) model->SetColor({ 0.38f, 0.64f, 0.78f, 1.0f });
	ModelManager::GetInstance()->CreateBoxModel("ObstacleBox");

	player_ = std::make_unique<Player>();
	player_->Initialize(kPlayerModelName);
	boundaryShieldSettings_.Apply(player_.get());
	CaptureGameplayBalanceDefaults();
	LoadGameplayBalanceSettings();
	ApplyGameplayBalanceSettings();


	missileManager_ = std::make_unique<MissileManager>();
	missileManager_->Initialize(environmentRenderer_->GetParticleManager());


	explosionManager_ = std::make_unique<ExplosionManager>();
	explosionManager_->Initialize(environmentRenderer_->GetParticleManager());

	enemies_.clear();

	enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
	enemyBulletManager_->Initialize();

	simulationManager_ = std::make_unique<SimulationManager>(this);
	simulationManager_->LoadBossThrusterSettings("resources/boss_thruster_settings.json", false);
	missilePresetManager_ = std::make_unique<MissilePresetManager>(MissilePresetSceneContext{
		player_.get(),
		ammoManager_.get(),
		missileManager_.get(),
		&missileNormalSpeed,
		&missileNormalScale,
		&missileNormalCollisionRadius,
		&missileNormalLifeTime,
		&missileSpeed,
		&missileHomingStrength,
		&missileHomingScale,
		&missileHomingCollisionRadius,
		&missileTrailWidth,
		&missileLifeTime,
		&missileMuzzleOffset,
		[this](MissileType type) { return MakeMissileTuning(type); },
		[this](MissileType type) { return TryConsumeAmmo(type); },
		[this]() {
			if (uiManager_) {
				uiManager_->DrawAmmoSettingsUI();
			}
		},
	});
	lockOnManager_ = std::make_unique<LockOnManager>(LockOnSceneContext{
		player_.get(),
		&enemies_,
		&targetSelection_,
		&multiLockChargeState_,
		missilePresetManager_.get(),
		&tutorialController_,
		&tutorialProgress_,
		[this]() { return IsTitleBackgroundMode(); },
		[this]() { ResetCinematicLockOnCamera(); },
	});
	cameraManager_ = std::make_unique<GameCameraManager>();


	gameOverSequence_.Reset();
	combatAlertState_.Reset();
	damageFeedbackState_.Reset();
	bossClearSequence_.Reset();
	defeatedEnemyCount_ = 0;
	gameplayElapsedFrames_ = 0;
	pauseMenuState_.Reset();
	returnToTitleTransition_.Reset();
	attractDemoController_.Reset(IsAttractDemoMode() && TitleScene::ShouldPlayClearCinematicDemo());
	// 通常のゲーム起動では、編集ツールの再生ボタンを押さなくても最初から操作できる。
	isEditorPreviewPlaying_ = true;

	ReloadSceneJson();
	// タイトル背景では、ステージ上の PLAYER 向きを一度表示せず、最初の描画から
	// 自動飛行コースの位置・姿勢にする。
	if (IsTitleBackgroundMode() && player_) {
		gameplayModeController_.PrimeTitleBackgroundPlayer(*player_, obstacles_);
	}
	if (!IsAutoFlightMode() && !IsAttractDemoMode()) {
		GameStartTransitionData titleTransition;
		if (GameStartTransition::Consume(titleTransition) && player_) {
			tutorialController_.Reset(titleTransition.stage == GameStartStage::Tutorial);
		tutorialProgress_.Reset();
			tutorialGoalMarker_.Reset();
			if (tutorialController_.IsEnabled()) {
				// 訓練中に本編の敵編成や増援イベントが介入しないよう、ステージから
				// 読み込んだ戦闘状態を空にしておく。攻撃フェーズで専用標的だけを出す。
				ClearAllEnemyReferences();
				enemies_.clear();
				enemySpawns_.clear();
				enemySpawnManager_.Clear();
				bossEncounterState_.Reset();
			}
			// ReloadSceneJson() で Blender の PLAYER オブジェクトを読み込んだ直後の
			// 座標・向きを保持する。タイトル側の飛行方向は使わないため、毎回同じ
			// ゲート位置から出て、演出終了時にもステージで定義した場所へ戻る。
			warpExitPresentation_.Start(*player_);

			// 到着後に演出用の前進を重ねない。これにより、壁へ押し出されず
			// Blender で指定した PLAYER の位置からゲームプレイを開始できる。
			introBoostState_.Reset(warpExitPresentation_.GetYaw());
		}
	}
	if (IsAttractDemoMode() && player_) {
		// タイトル背景の周回軌道ではなく、通常ステージに置かれたPLAYERの
		// 進行方向を使う。敵構成と攻撃もゲームプレイのものをそのまま使う。
		attractDemoController_.InitializeFlightDirection(player_->GetForwardVector());
	}

	warpExitPresentation_.InitializeGateObjects();

	// 障害物・地形のコライダーを全件有効化して更新（PDF 不具合A-1, A-2対策）
	for (auto& obstacle : obstacles_) {
		if (obstacle) {
			obstacle->SetCollisionEnabled(true);
			obstacle->Update();
		}
	}

	// プレイヤーの移動制御フラグと推力・コライダー復帰（PDF 不具合B-1, A-3対策）
	if (player_) {
		player_->SetSpecialAttackActive(false);
		player_->ChangeMode(PlayerMode::Fighter);

		// ファイター形態の前進基本推力を直ちに付与し、前進停止を防止
		const Vector3 forward = player_->GetForwardVector();
		const float baseSpeed = player_->GetModeParams(PlayerMode::Fighter).maxMoveSpeed * 0.8f;
		player_->SetVelocity({ forward.x * baseSpeed, forward.y * baseSpeed, forward.z * baseSpeed });

		// カメラを自機位置に即座に同期（PDF 不具合B-3対策）
		if (camera) {
			player_->UpdateCamera(camera.get(), nullptr, false);
			camera->Update();
		}
	}

	EditorReceiver::GetInstance()->Initialize();
}

void GamePlayScene::SetDebugCameraActive(bool isActive) {
	if (isDebugCameraActive_ == isActive) {
		return;
	}

	isDebugCameraActive_ = isActive;
	ResetCinematicLockOnCamera();
	if (isDebugCameraActive_) {
		ResetDebugCameraToStageOverview();
		Object3dCommon::GetInstance()->SetDefaultCamera(debugFlyCamera_.get());
		OutputDebugStringA("[DebugCamera] ON: FlyCamera\n");
	} else {
		Object3dCommon::GetInstance()->SetDefaultCamera(camera.get());
		OutputDebugStringA("[DebugCamera] OFF: Player Camera\n");
	}
}

void GamePlayScene::ResetDebugCameraToPlayer() {
	debugEditorController_.ResetCameraToPlayer(debugFlyCamera_.get(), camera.get(), player_.get());
}

void GamePlayScene::ResetDebugCameraToStageOverview() {
	debugEditorController_.ResetCameraToStageOverview(debugFlyCamera_.get(), camera.get(), player_.get(), obstacles_);
}

void GamePlayScene::ReloadSceneJson() {
	ClearNormalEnemyDebris();
	stageReloadController_.Reload(
		IsAutoFlightMode(),
		player_.get(),
		enemies_,
		obstacles_,
		enemySpawns_,
		bossEncounterState_,
		enemySpawnManager_,
		enemyEventManager_,
		titleBackgroundEnemyState_,
		lockOnManager_.get(),
		[this]() { ClearAllEnemyReferences(); });

	// StageLoader はゲーム状態だけを更新する。描画モデルは古い座標・姿勢のまま
	// 1 フレーム描かれ得るため、タイトル復帰・ゲーム開始を含めて即座に同期する。
	if (player_) {
		player_->UpdateModel(false, false);
	}
}

void GamePlayScene::ResetEditorPreview() {
	// チュートリアルの敵は scene.json ではなく BeginTutorialCombat() が専用に生成する。
	// ここで通常ステージの scene.json を読み直すと、その訓練用編成がステージ1の敵へ
	// 置き換わってしまうため、訓練中は現在の配置を保ったまま編集停止へ移行する。
	const bool preserveTutorialLayout = tutorialController_.IsEnabled() && tutorialController_.GetPhase() != TutorialPhase::Disabled;
	Vector3 tutorialPlayerPosition{};
	Vector3 tutorialPlayerForward{};
	PlayerMode tutorialPlayerMode = PlayerMode::Fighter;
	if (preserveTutorialLayout && player_) {
		tutorialPlayerPosition = player_->GetPosition();
		tutorialPlayerForward = player_->GetForwardVector();
		tutorialPlayerMode = player_->GetCurrentMode();
	}

	isEditorPreviewPlaying_ = false;
	gameOverSequence_.Reset();
	combatAlertState_.Reset();
	damageFeedbackState_.Reset();
	bossClearSequence_.Reset();
	ClearNormalEnemyDebris();
	ClearAllEnemyReferences();
	if (lockOnManager_) {
		lockOnManager_->CancelMultiLock();
	}

	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	if (player_) {
		player_->Initialize(kPlayerModelName);
	}
	if (missileManager_) {
		missileManager_->Initialize(environmentRenderer_->GetParticleManager());
	}
	if (enemyBulletManager_) {
		enemyBulletManager_->Initialize();
	}
	if (explosionManager_) {
		explosionManager_->Initialize(environmentRenderer_->GetParticleManager());
	}

	if (preserveTutorialLayout) {
		// Initialize() によるプレイヤーの初期位置への巻き戻しも防ぐ。
		if (player_) {
			player_->SetPosition(tutorialPlayerPosition);
			player_->ChangeMode(tutorialPlayerMode);
			const float horizontalLength = std::sqrt(
				tutorialPlayerForward.x * tutorialPlayerForward.x +
				tutorialPlayerForward.z * tutorialPlayerForward.z);
			if (horizontalLength > 0.0001f) {
				const float yaw = std::atan2(tutorialPlayerForward.x, tutorialPlayerForward.z);
				player_->SetRotation({ 0.0f, yaw, 0.0f });
			}
		}
	} else {
		ReloadSceneJson();
	}

	if (!isDebugCameraActive_ && player_) {
		Vector3* targetPos = nullptr;
		Vector3 enemyPos;
		if (Enemy* lockedEnemy = targetSelection_.GetLockedEnemy()) {
			enemyPos = lockedEnemy->GetPosition();
			targetPos = &enemyPos;
		}
		player_->UpdateCamera(camera.get(), targetPos);
	}

	// ツールバーは UpdateUI() の末尾で処理されるため、次フレームを待たず
	// 再配置・カメラ更新後にモデルと炎を準備する。両シーン共通の編集処理。
	if (player_) {
		player_->UpdateModel(false, false);
		if (player_->GetBoosterEffect()) {
			player_->GetBoosterEffect()->UpdateEditorPreview(player_->GetPosition(), player_->GetQuaternion(),
				static_cast<int>(player_->GetCurrentMode()));
		}
	}
	OutputDebugStringA("[EditorPreview] Reset scene and paused.\n");
}





























MissileTuning GamePlayScene::MakeMissileTuning(MissileType type) const {
	MissileTuning tuning;
	if (type == MissileType::Normal) {
		tuning.speed = missileNormalSpeed;
		// ロックオン中の通常弾だけに使う、見た目を崩さない弱い追尾力。
		tuning.homingStrength = missileNormalHomingStrength;
		tuning.scale = missileNormalScale;
		tuning.collisionRadius = missileNormalCollisionRadius;
		tuning.trailWidth = 0.0f;
		tuning.lifeTime = missileNormalLifeTime;
		return tuning;
	}

	tuning.speed = missileSpeed;
	tuning.homingStrength = missileHomingStrength;
	tuning.scale = missileHomingScale;
	tuning.collisionRadius = missileHomingCollisionRadius;
	tuning.trailWidth = missileTrailWidth;
	tuning.lifeTime = missileLifeTime;
	return tuning;
}































void GamePlayScene::Finalize() {
	if (pVoice1) {
		pVoice1->Stop();
		pVoice1->DestroyVoice();
		pVoice1 = nullptr;
	}
	if (pVoice2) {
		pVoice2->Stop();
		pVoice2->DestroyVoice();
		pVoice2 = nullptr;
	}
	if (pSongVoice) {
		pSongVoice->Stop();
		pSongVoice->DestroyVoice();
		pSongVoice = nullptr;
	}

	AudioManager::GetInstance()->UnloadWave(soundData1);
	AudioManager::GetInstance()->UnloadWave(soundData2);
	AudioManager::GetInstance()->UnloadWave(songSoundData);


	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	EditorReceiver::GetInstance()->Finalize();
}

bool GamePlayScene::IsPauseBlockedByCinematic() const {
	return IsTitleBackgroundMode() ||
		gameOverSequence_.IsActive() ||
		bossClearSequence_.IsActive() ||
		bossEncounterState_.IsIntroActive() ||
		warpExitPresentation_.IsActive() ||
		introBoostState_.IsActive() ||
		returnToTitleTransition_.IsActive();
}

void GamePlayScene::CancelPauseForCinematic() {
	if (!pauseMenuState_.IsPaused()) {
		return;
	}
	pauseMenuState_.Resume();
	// ポーズ開始時に止めた歌も、演出時間の進行と同じ状態へ戻す。
	if (songState_.IsActive() && pSongVoice) {
		pSongVoice->Start();
	}
}

bool GamePlayScene::UpdatePauseMenu(bool canUseKeyboardInput) {
	if (IsPauseBlockedByCinematic()) {
		CancelPauseForCinematic();
		return false;
	}
	if (!canUseKeyboardInput) {
		return false;
	}

	Input* input = Input::GetInstance();
	const bool togglePause = input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_START);
	const bool cancelPause = pauseMenuState_.IsPaused() && input->TriggerControllerButton(XINPUT_GAMEPAD_B);

	if (togglePause || cancelPause) {
		const bool isNowPaused = pauseMenuState_.Toggle();
		// 歌のループ音もゲーム進行と同じタイミングで止める。
		if (songState_.IsActive() && pSongVoice) {
			if (isNowPaused) {
				pSongVoice->Stop();
			} else {
				pSongVoice->Start();
			}
		}
		return false;
	}

	if (!pauseMenuState_.IsPaused()) {
		return false;
	}

	const bool navMove = input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_DOWN) ||
		input->TriggerKey(DIK_W) || input->TriggerKey(DIK_S) ||
		input->TriggerControllerInput(ControllerInput::DPadUp) || input->TriggerControllerInput(ControllerInput::DPadDown) ||
		input->TriggerControllerInput(ControllerInput::LeftStickUp) || input->TriggerControllerInput(ControllerInput::LeftStickDown);
	if (navMove) {
		pauseMenuState_.ToggleSelection();
	}

	const bool confirm = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
		input->TriggerControllerButton(XINPUT_GAMEPAD_A);
	if (!confirm) {
		return false;
	}

	if (pauseMenuState_.IsTitleSelected()) {
		StartReturnToTitleTransition();
		return true;
	}

	pauseMenuState_.Resume();
	if (songState_.IsActive() && pSongVoice) {
		pSongVoice->Start();
	}
	return false;
}

void GamePlayScene::ApplyAudioSettings() {
	const auto stopVoice = [](IXAudio2SourceVoice*& voice) {
		if (voice) {
			voice->Stop();
			voice->DestroyVoice();
			voice = nullptr;
		}
	};

	// タイトル背景は常に無音。本編・デモだけが音声設定の対象になる。
	if (IsTitleBackgroundMode()) {
		stopVoice(pVoice1);
		stopVoice(pVoice2);
		stopVoice(pSongVoice);
		return;
	}

	const GameSettings& settings = GameSettings::GetInstance();
	if (settings.IsSoundEffectsEnabled()) {
		if (!pVoice1) {
			pVoice1 = AudioManager::GetInstance()->PlayWave(soundData1, true);
		}
	} else {
		stopVoice(pVoice1);
	}

	if (!settings.IsGameplayBgmEnabled()) {
		stopVoice(pVoice2);
		stopVoice(pSongVoice);
		return;
	}

	if (!pVoice2) {
		pVoice2 = AudioManager::GetInstance()->PlayWave(soundData2, true);
	}
	if (!pSongVoice) {
		pSongVoice = AudioManager::GetInstance()->PlayWave(songSoundData, true);
	}

	const bool songIsActive = songState_.IsActive();
	if (pVoice2) {
		pVoice2->SetVolume(songIsActive ? 0.0f : 1.0f);
	}
	if (pSongVoice) {
		pSongVoice->SetVolume(songIsActive ? 1.0f : 0.0f);
	}
}

void GamePlayScene::Update() {
	ApplyAudioSettings();
	combatAlertState_.Update();
	Camera* activeCamera = isDebugCameraActive_ ? static_cast<Camera*>(debugFlyCamera_.get()) : camera.get();
	if (activeCamera) {
		Object3dCommon::GetInstance()->SetDefaultCamera(activeCamera);
	}

	if (warpFadeInAlpha_ > 0.0f) {
		warpFadeInAlpha_ -= 1.0f / 35.0f;
		if (warpFadeInAlpha_ <= 0.0f) {
			warpFadeInAlpha_ = 0.0f;
		}
	}

	if (EditorReceiver::GetInstance()->Update(player_.get(), enemies_, obstacles_, enemySpawns_)) {
		// エディタからの差し替えでは敵リスト全体が再生成されるため、生ポインタの参照を残さない。
		ClearAllEnemyReferences();
		// Blenderで設定された初期スポーン設定(isInitialSpawn)をそのまま尊重する
	}

	const GameplayRunMode runMode = IsTitleBackgroundMode()
		? GameplayRunMode::TitleBackground
		: (IsAttractDemoMode() ? GameplayRunMode::AttractDemo : GameplayRunMode::Gameplay);
	const GameplayInputPermissions inputPermissions = gameplayModeController_.EvaluateInputPermissions(
		runMode, IsImGuiKeyboardCaptureActive(), IsImGuiMouseCaptureActive());
	const bool canUseKeyboardInput = inputPermissions.canUseKeyboardInput;
	const bool canUsePlayerInput = inputPermissions.canUsePlayerInput;
	// アトラクトデモは、演出中・撃破演出中を問わず TAB で即座にタイトルへ戻れる。
	if (IsAttractDemoMode() && Input::GetInstance()->TriggerKey(DIK_TAB)) {
		TitleScene::StopAttractDemo();
		TitleScene::SetPendingFadeIn(true);
		SceneManager::GetInstance()->ChangeScene("TITLE");
		return;
	}
	// ポーズメニューからの通信切断・システムシャットダウン演出進行中
	if (returnToTitleTransition_.IsActive()) {
		UpdateReturnToTitleTransition();
		return;
	}

	if (UpdatePauseMenu(canUseKeyboardInput)) {
		return;
	}

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_0)) {
		OutputDebugStringA("HIt 0\n");
	}

	if (!pauseMenuState_.IsPaused() && canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_R)) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
		return;
	}

	// ==========================================

	// ==========================================
	if (IsAttractDemoMode() && !gameOverSequence_.IsActive()) {
		attractDemoController_.UpdateSequence(
			player_.get(), enemies_, enemySpawns_, enemySpawnManager_, bossEncounterState_,
			[this]() { ClearAllEnemyReferences(); });
	}

	if (!IsTitleBackgroundMode() && !gameOverSequence_.IsActive() &&
		(!IsAttractDemoMode() || !attractDemoController_.IsClearCinematic()) &&
		!bossClearSequence_.IsActive() && player_ && player_->IsDead()) {
		gameOverSequence_.Start(player_->GetPosition());
		// 撃墜したフレームに ESC が重なっても、ゲームオーバー演出を停止させない。
		CancelPauseForCinematic();
		MissionResultStore::Set({ MissionResultOutcome::Failed, defeatedEnemyCount_, player_->GetHP(), gameplayElapsedFrames_,
			IsAttractDemoMode(), tutorialController_.IsEnabled() ? MissionStage::Tutorial : MissionStage::Standard });
		player_->SetPresentationThrustersVisible(false);

		std::vector<Vector3> playerHitPos = { player_->GetPosition() };
		if (explosionManager_) {
			explosionManager_->CreateDestructionEffects(
				playerHitPos, ExplosionManager::EffectTarget::Player);
		}

		if (pVoice2) {
			pVoice2->Stop();
		}
	}

	const bool isBossClearSequenceActive = bossClearSequence_.IsActive();
	if (isBossClearSequenceActive) {
		// ボス撃破と ESC が同フレームでも、墜落・爆散シーケンスを必ず進行させる。
		CancelPauseForCinematic();
		ParticleManager* particles = environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr;
		if (bossClearSequence_.Update(
				pauseMenuState_.IsPaused(),
				enemies_,
				obstacles_,
				player_.get(),
				explosionManager_.get(),
				particles,
				[this](Enemy* enemy) { ClearEnemyReferences(enemy); })) {
			MissionResultStore::Set({ MissionResultOutcome::Clear, defeatedEnemyCount_, player_ ? player_->GetHP() : 0, gameplayElapsedFrames_,
				IsAttractDemoMode() });
			SceneManager::GetInstance()->ChangeScene("CLEAR");
			return;
		}
	}

	bool shouldUpdateGame = !pauseMenuState_.IsPaused() && !isBossClearSequenceActive;
	const bool isMouseLookActive = gameplayModeController_.ShouldClipMouseCursor(
		runMode, gameOverSequence_.IsActive(), returnToTitleTransition_.IsActive(),
		shouldUpdateGame, canUsePlayerInput, ImGuiManager::IsVisible());
	Input::GetInstance()->SetMouseCursorClipEnabled(isMouseLookActive);

	if (gameOverSequence_.IsActive()) {
		gameOverSequence_.Advance();
		// 装甲の各所が時間差で破裂するよう、機体中心の周りに爆発を散らす。
		// 更新頻度も1/5にすることで、世界全体がスローになる演出を作る。
		if (explosionManager_ && gameOverSequence_.GetFrame() <= 108 && gameOverSequence_.GetFrame() % 18 == 0) {
			const float burstIndex = static_cast<float>(gameOverSequence_.GetFrame() / 18);
			const Vector3 burstPosition = {
				gameOverSequence_.GetFocusPosition().x + std::cos(burstIndex * 2.41f) * 1.75f,
				gameOverSequence_.GetFocusPosition().y + std::sin(burstIndex * 1.73f) * 1.10f,
				gameOverSequence_.GetFocusPosition().z + std::sin(burstIndex * 2.41f) * 1.75f,
			};
			explosionManager_->CreateDestructionEffects(
				{ burstPosition }, ExplosionManager::EffectTarget::Player);
			if (environmentRenderer_) {
				if (ParticleManager* particles = environmentRenderer_->GetParticleManager()) {
					particles->EmitDirectionalBurst("test", burstPosition,
						SubtractVector3(burstPosition, gameOverSequence_.GetFocusPosition()), 180,
						{ 1.0f, 0.34f, 0.05f, 0.95f }, 1.15f,
						1.35f, 0.75f, 0.055f, 0.035f, 0.70f, 1.45f, 0.95f);
				}
			}
		}


		if (PostEffect::GetInstance()) {
			float effectProgress = static_cast<float>(gameOverSequence_.GetFrame()) / 120.0f;
			if (effectProgress > 1.0f) {
				effectProgress = 1.0f;
			}
			float vignetteRadius = 0.62f - 0.22f * effectProgress;
			float blurIntensity = 1.5f + 3.0f * effectProgress;
			PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.38f, blurIntensity);
		}


		shouldUpdateGame = (gameOverSequence_.GetFrame() % 5 == 0);


		if (gameOverSequence_.HasFinished()) {
			SceneManager::GetInstance()->ChangeScene("GAMEOVER");
		}
	} else {
		if (!IsTitleBackgroundMode() && shouldUpdateGame) {
			++gameplayElapsedFrames_;
		}

		playerFlightEffectController_.UpdatePostEffect(player_.get(), damageFeedbackState_);
	}

	// 上部ツールバーの「再生／編集に戻す」は、通常ゲーム画面と
	// シミュレーション画面のどちらでも共通の停止スイッチとして扱う。
	// 以前は通常ゲーム画面ではこのフラグを無視していたため、
	// 「編集モード（停止中）」でもゲームだけ進行してしまっていた。
	shouldUpdateGame = shouldUpdateGame && isEditorPreviewPlaying_;
	// ボス出現の警告中は一時停止と同様にゲーム進行を止める。ただしフレーム数は
	// 通常のゲーム進行時だけ減らし、ポーズ中に演出が消えないようにする。
	if (shouldUpdateGame && bossEncounterState_.IsIntroActive()) {
		bossEncounterState_.AdvanceIntro();
	}
	const bool isBossIntroSequenceActive = bossEncounterState_.IsIntroActive();
	const GameplayFrameUpdatePolicy framePolicy = gameplayModeController_.EvaluateFrameUpdatePolicy({
		shouldUpdateGame,
		isBossIntroSequenceActive,
		warpExitPresentation_.IsActive(),
		introBoostState_.IsActive(),
		gameOverSequence_.IsActive(),
		canUseKeyboardInput,
		canUsePlayerInput,
	});
	const bool updateSelectedPlayer = framePolicy.updateSelectedPlayer;
	const bool updateSelectedMissiles = framePolicy.updateSelectedMissiles;
	const bool updateSelectedEnemies = framePolicy.updateSelectedEnemies;
	const bool updateSelectedParticles = framePolicy.updateSelectedParticles;
	const bool allowWeaponInput = framePolicy.allowWeaponInput;
	const bool allowLockOnBehavior = framePolicy.allowLockOnBehavior;
	const bool updateAnimationPreview = framePolicy.updateAnimationPreview;

	playerWeaponController_.UpdateSpecialAndSong({
		shouldUpdateGame,
		canUsePlayerInput,
		gameOverSequence_.IsActive(),
		&gameplayBalanceSettings_.Get(),
		player_.get(),
		&enemies_,
		&targetSelection_,
		ammoManager_.get(),
		missilePresetManager_.get(),
		environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr,
		&songFlightTrailRenderer_,
		&specialAttackState_,
		&songState_,
		pVoice2,
		pSongVoice,
	});

	if (myBox && animationData.duration > 0.0f) {
		if (playAnimation && updateAnimationPreview) {
			animationTime += 1.0f / 60.0f;
			animationTime = std::fmod(animationTime, animationData.duration);
		}
		

		ApplyAnimation(skeleton, animationData, animationTime);
		::Update(skeleton);
		if (enableSkinning && myModelObject->GetModel()) {
			myModelObject->GetModel()->UpdateSkinCluster(myModelObject->skinCluster, skeleton);
		}


		if (!skeleton.joints.empty()) {
			myBox->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
			myBox->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
			myBox->SetScale(skeleton.joints[skeleton.root].transform.scale);



			if (myModelObject->GetModel()) {
				if (!myModelObject->skinCluster.isValid) {
					myModelObject->SetTranslate(skeleton.joints[skeleton.root].transform.translate);
					myModelObject->SetQuaternionRotate(skeleton.joints[skeleton.root].transform.rotate);
					myModelObject->SetScale(skeleton.joints[skeleton.root].transform.scale);
				} else {


					myModelObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
					myModelObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
					myModelObject->SetScale({ modelScale, modelScale, modelScale });
				}
			}
		}


		if (showBones && player_) {
			std::vector<VertexData> lineVertices;

			const Skeleton& playerSkeleton = player_->GetSkeleton();
			for (size_t i = 0; i < playerSkeleton.joints.size(); ++i) {
				Vector3 pos = {
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][0],
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][1],
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][2]
				};


				if (playerSkeleton.joints[i].parent) {
					int32_t parentIndex = *playerSkeleton.joints[i].parent;
					Vector3 parentPos = {
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][0],
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][1],
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][2]
					};

					VertexData v1, v2;
					v1.position = { parentPos.x, parentPos.y, parentPos.z, 1.0f };
					v1.normal = { 0.0f, 1.0f, 0.0f };
					v1.texcoord = { 0.0f, 0.0f };

					v2.position = { pos.x, pos.y, pos.z, 1.0f };
					v2.normal = { 0.0f, 1.0f, 0.0f };
					v2.texcoord = { 1.0f, 1.0f };

					Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
					if (simulationManager_ && simulationManager_->IsBoneSelected(playerSkeleton.joints[i].name)) {
						v1.color = { 1.0f, 1.0f, 0.0f, 1.0f };
						v2.color = { 1.0f, 1.0f, 0.0f, 1.0f };
					} else {
						v1.color = color;
						v2.color = color;
					}

					lineVertices.push_back(v1);
					lineVertices.push_back(v2);
				}
			}



			if (player_ && player_->GetObject3d()) {
				skeletonLinesObject->SetTranslate(player_->GetPosition());
				skeletonLinesObject->SetQuaternionRotate(player_->GetQuaternion());
				skeletonLinesObject->SetScale(player_->GetObject3d()->GetScale());
			}
			if (!lineVertices.empty() && skeletonLinesObject->GetModel()) {
				skeletonLinesObject->GetModel()->UpdateLineVertices(lineVertices);
			}
			skeletonLinesObject->Update();
		}
	}


	if (showModel && myModelObject) {
		myModelObject->Update();
	}

	// Debug camera switching is handled by ImGui buttons in UpdateUI().
	if (false && Input::GetInstance()->TriggerKey(DIK_F1)) {
		SetDebugCameraActive(!isDebugCameraActive_);
	}



	activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	lockOnManager_->UpdateLockOn(activeCamera, allowLockOnBehavior);


	gameplayModeController_.UpdatePlayerForMode({
		runMode,
		updateSelectedPlayer,
		canUseKeyboardInput,
		shouldUpdateGame,
		isEditorPreviewPlaying_,
		player_.get(),
		&obstacles_,
		targetSelection_.GetLockedEnemy(),
		lockOnManager_.get(),
		&titleLaunchState_,
		&attractDemoController_,
		&warpExitPresentation_,
		&introBoostState_,
		environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr,
	});

	// ==========================================

	// ==========================================

	Vector3 playerPos = player_ ? player_->GetOBB().center : Vector3{ 0.0f, 0.0f, 0.0f };

	if (updateSelectedEnemies) {

		ParticleManager* particleManager = environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr;
		enemyUpdateCoordinator_.UpdateEnemies({
			IsAutoFlightMode(),
			gameplayBalanceSettings_.Get().bossMaxActiveMinions,
			player_.get(),
			&enemies_,
			&obstacles_,
			enemyBulletManager_.get(),
			explosionManager_.get(),
			particleManager,
			&combatAlertState_,
			[this](GroundEnemy& enemy) { SpawnNormalEnemyDebris(enemy); },
			[this](GroundEnemy& enemy) { SpawnHomingEnemyImplosion(enemy); },
		}, playerPos);
		enemyUpdateCoordinator_.ProcessDefeatedEnemies({
			IsAutoFlightMode(),
			gameOverSequence_.IsActive(),
			player_.get(),
			&enemies_,
			&enemySpawns_,
			&enemyEventManager_,
			&enemySpawnManager_,
			&songState_,
			&bossClearSequence_,
			&titleBackgroundEnemyState_,
			&defeatedEnemyCount_,
			[this](Enemy* enemy) { ClearEnemyReferences(enemy); },
		});

		if (IsAutoFlightMode() && player_) {
			// 生存中の敵を選び、全滅時は待機後にタイトル背景用の敵を再出現させる。
			if (!targetSelection_.HasAliveLockedEnemy()) {
				targetSelection_.SetLockedTarget(
					titleBackgroundEnemyController_.AcquireTarget(*player_, enemies_, titleBackgroundEnemyState_), -1);
			}

			if (Enemy* lockedEnemy = targetSelection_.GetLockedEnemy()) {
				titleBackgroundEnemyFlightController_.Update(*player_, *lockedEnemy, *camera,
					obstacles_, titleBackgroundEnemyState_, gameplayModeController_.GetTitleBackgroundFrame());
				titleBackgroundEnemyWeaponController_.Update(
					*lockedEnemy, titleBackgroundEnemyState_, missilePresetManager_.get());
			}
		}

		Vector3 targetBossPos = playerPos;
		const Boss::Tuning* bossTuning = nullptr;
		if (uiManager_) {
			targetBossPos = uiManager_->GetBossBattlePosition();
			bossTuning = &uiManager_->GetBossTuning();
		}
		if (enemyUpdateCoordinator_.UpdateWaveAndBossProgression({
				IsAutoFlightMode(),
				gameOverSequence_.IsActive(),
				targetBossPos,
				bossTuning,
				&enemySpawns_,
				&enemies_,
				&obstacles_,
				&enemySpawnManager_,
				&bossEncounterState_,
			})) {
			SceneManager::GetInstance()->ChangeScene("CLEAR");
			return;
		}


		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	} else {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
	}

	debugEditorController_.UpdateBossEditorPartSelection(
		isDebugCameraActive_ ? static_cast<Camera*>(debugFlyCamera_.get()) : camera.get(),
		isEditorPreviewPlaying_, uiManager_.get(), enemies_);

	if (isDebugCameraActive_) {
		debugFlyCamera_->SetCanUseKeyboard(canUseKeyboardInput);
		debugFlyCamera_->Update();

		// Object3d は Update 時点のカメラ行列を WVP に保持する。
		// 停止中のプレビューでも、フリーカメラを動かした後に地形を含む
		// 障害物の WVP を更新しないと、旧カメラの位置でカリングされてしまう。
		for (auto& obstacle : obstacles_) {
			obstacle->Update();
		}
		debugEditorController_.TryPlaceEnemyAtGameViewMouse(debugFlyCamera_.get(), uiManager_.get(), simulationManager_.get());
	} else {
		if (cinematicCameraCoordinator_.TryUpdate(
			camera.get(), isDebugCameraActive_,
			gameOverSequence_, bossClearSequence_.GetPhaseState(), bossClearSequence_.GetTimeline(),
			bossClearSequence_.GetFocusState(), bossClearSequence_.GetCameraState(),
			BossClearSequenceController::kPhase1DurationSec, BossClearSequenceController::kPhase2FlashDurationSec)) {
		} else if (player_) {
			if (IsAutoFlightMode()) {
				// 出撃中もタイトル時と同じ追従カメラを使い、地形と飛行方向を連続させる。
				// タイトル背景でも機体を見失わないよう、通常のプレイヤー追従カメラを使う。
				// 待機中は緩やかに揺らし、出撃時はワープに吸い込まれるよう視野角を一気に広げる。
				const float time = static_cast<float>(gameplayModeController_.GetTitleBackgroundFrame()) * (1.0f / 60.0f);
				player_->UpdateCamera(camera.get(), nullptr, false);
				if (titleLaunchState_.IsActive()) {
					const float launchProgress = std::clamp(
						static_cast<float>(titleLaunchState_.GetFrame()) / static_cast<float>(TitleLaunchState::kDurationFrames),
						0.0f, 1.0f);
					const float fovProgress = std::clamp((launchProgress - 0.16f) / 0.62f, 0.0f, 1.0f);
					const float easedProgress = fovProgress * fovProgress * (3.0f - 2.0f * fovProgress);
					const float impactProgress = std::clamp(
						1.0f - std::abs(launchProgress - 0.77f) / 0.15f, 0.0f, 1.0f);
					// 衝撃と同時にわずかに視野を押し広げ、加速の重さを強調する。
					camera->SetFovY(0.60f + easedProgress * 0.62f + impactProgress * impactProgress * 0.09f);
					const Vector3 shake = CalculateTitleLaunchCameraShake(launchProgress, titleLaunchState_.GetFrame());
					const Vector3 basePosition = camera->GetTranslate();
					camera->SetTranslate({
						basePosition.x + shake.x,
						basePosition.y + shake.y,
						basePosition.z + shake.z,
					});
				} else {
					camera->SetFovY(0.60f + std::sin(time * 0.55f) * 0.035f);
				}
			} else {
				Vector3* targetPos = nullptr;
				Vector3 enemyPos;
				if (targetSelection_.HasLockedEnemy() && lockOnManager_ && lockOnManager_->IsLockedEnemyAlive()) {
					enemyPos = lockOnManager_->GetLockedTargetPosition();
					targetPos = &enemyPos;
				}
				player_->UpdateCamera(camera.get(), targetPos);
				if (warpExitPresentation_.IsActive() && !isDebugCameraActive_) {
					warpExitPresentation_.UpdateCamera(camera.get(), player_.get());
				} else if (cameraManager_ && cameraManager_->IsCinematicLockOnEnabled() &&
					targetSelection_.HasAliveLockedEnemy()) {
					cameraManager_->UpdateCinematicLockOnCamera(camera.get(), targetSelection_.GetLockedEnemy(), player_->GetPosition());
				} else if (cameraManager_) {
					cameraManager_->ResetCinematicCamera();
				}
			}
		}
		camera->Update();
	}

	// Object3d は Update 時のカメラ行列を WVP として保持する。地形は物理更新時にも
	// 更新されるが、ワープ演出ではこの後にカメラが大きく移動するため、確定した
	// カメラ行列でもう一度更新して地面がカメラに固定されたように見えるのを防ぐ。
	for (auto& obstacle : obstacles_) {
		if (obstacle) {
			obstacle->Update();
		}
	}

	playerFlightEffectController_.UpdateAfterPlayerMovement({
		shouldUpdateGame,
		gameOverSequence_.IsActive(),
		IsTitleBackgroundMode(),
		gameplayElapsedFrames_,
		&gameplayBalanceSettings_.Get(),
		player_.get(),
		activeCamera,
		environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr,
		&songState_,
		&songFlightTrailRenderer_,
		&warpExitPresentation_,
		&obstacles_,
	});

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	Vector2 size = sprite->GetSize();
	size.x = 300.0f;
	size.y = 300.0f;
	sprite->SetSize(size);

	if (environmentRenderer_->GetShowParticles() && updateSelectedParticles) {
	}


	// ==========================================

	// ==========================================
	playerWeaponController_.UpdateWeaponInput({
		allowWeaponInput,
		gameOverSequence_.IsActive(),
		player_.get(),
		activeCamera,
		targetSelection_.GetLockedEnemy(),
		targetSelection_.GetAimAssistEnemy(),
		lockOnManager_.get(),
		missilePresetManager_.get(),
		&specialAttackState_,
		&multiLockChargeState_,
		&tutorialController_,
		&tutorialProgress_,
	});

	// ==========================================

	// ==========================================
	std::vector<Vector3> hitPositions;
	std::vector<Vector3> homingHitPositions;
	std::vector<MissileManager::ShieldImpact> shieldImpacts;
	std::vector<MissileManager::DefeatedEnemy> destroyedEnemies;
	if (updateSelectedMissiles) {
		if (missileManager_) {
			missileManager_->Update(activeCamera, enemies_, obstacles_, hitPositions, homingHitPositions, shieldImpacts, destroyedEnemies);
		}

		if (explosionManager_ && !hitPositions.empty()) {
			explosionManager_->CreateHitEffects(hitPositions);
		}
		if (explosionManager_ && !homingHitPositions.empty()) {
			explosionManager_->CreateHomingHitEffects(homingHitPositions);
		}
		if (ParticleManager* particleManager = environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr) {
			for (const MissileManager::ShieldImpact& shieldImpact : shieldImpacts) {
				// 側面シールドは本体の橙色爆発ではなく、法線方向へ青白い電気火花を散らす。
				particleManager->EmitDirectionalBurst("test", shieldImpact.position, shieldImpact.normal, 22,
					{ 0.42f, 0.82f, 1.0f, 0.84f }, 0.52f,
					1.55f, 0.36f, 0.038f, 0.016f, 0.12f, 0.24f, 0.10f);
				particleManager->EmitFireball("test", shieldImpact.position, 1,
					{ 0.78f, 0.94f, 1.0f, 0.72f }, 0.12f, 0.44f, 0.09f, 0.0f);
			}
		}
		for (const MissileManager::DefeatedEnemy& defeatedEnemy : destroyedEnemies) {
			Enemy* destroyedEnemy = defeatedEnemy.enemy;
			if (!destroyedEnemy) {
				continue;
			}

			// 通常弾では通常敵の機体を撃ち砕き、リングを出さずに破片だけを飛散させる。
			if (defeatedEnemy.projectileType == MissileType::Normal && !destroyedEnemy->IsBoss()) {
				SpawnNormalEnemyDebris(*destroyedEnemy);
				// 誘導弾は敵モデルとリングを中心へ収束させて消滅させる。
			} else if (defeatedEnemy.projectileType == MissileType::MissileWithTrail && !destroyedEnemy->IsBoss()) {
				SpawnHomingEnemyImplosion(*destroyedEnemy);
				if (explosionManager_) {
					explosionManager_->CreateImplosionEffects({ destroyedEnemy->GetAimPosition() });
				}
			} else if (explosionManager_) {
				// ボスは既存の演出を維持する。
				explosionManager_->CreateDestructionEffects(
					{ destroyedEnemy->GetAimPosition() },
					ExplosionManager::ClassifyEnemy(*destroyedEnemy));
			}
		}
		UpdateNormalEnemyDebris();
		UpdateHomingEnemyImplosions();

	} else {
		if (missileManager_) {
			missileManager_->UpdateModels(activeCamera);
		}
	}


	if ((shouldUpdateGame || isBossClearSequenceActive || updateSelectedMissiles || updateSelectedParticles ||
		(uiManager_ && uiManager_->IsDestructionEffectPreviewActive())) && explosionManager_) {
		explosionManager_->Update();
	}

	// ==========================================

	// ==========================================
	debugColliderRenderer_.Update({
		showDebugColliders,
		true,
		true,
		uiManager_ ? uiManager_->GetCurrentSimulationTarget() : 0,
		player_.get(),
		&obstacles_,
		&enemies_,
		targetSelection_.GetLockedEnemy(),
		missileManager_.get(),
		enemyBulletManager_.get(),
	});

	if (environmentRenderer_) {
		// 編集モード停止中は、パーティクル・UVスクロールを含む環境演出も止める。
		const bool updateEnvironment = shouldUpdateGame || isBossClearSequenceActive ||
			(gameOverSequence_.IsActive() && gameOverSequence_.GetFrame() % 5 == 0) ||
			(uiManager_ && uiManager_->IsDestructionEffectPreviewActive());
		environmentRenderer_->Update(activeCamera, updateEnvironment);
	}

	UpdateTutorial();

#ifdef ENABLE_IMGUI
	// タイトルは GamePlayScene を背景として使う。ゲーム専用の編集／Simulation UI を
	// ここで描くと、ポーズからタイトルへ戻った後にも本編のパネルだけが残る。
	if (uiManager_ && !IsTitleBackgroundMode() && !returnToTitleTransition_.IsActive()) {
		uiManager_->UpdateUI();
	}
#endif
	sprite->Update();
}

void GamePlayScene::Draw() {

	Camera* renderCamera = isDebugCameraActive_ ? static_cast<Camera*>(debugFlyCamera_.get()) : camera.get();
	if (!renderCamera) {
		return;
	}
	Object3dCommon::GetInstance()->SetDefaultCamera(renderCamera);

	// 深度を書き込まない炎やパーティクルを、背景で上書きしない。
	if (environmentRenderer_) {
		environmentRenderer_->DrawBackground();
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();


	const bool shouldDrawPlayer = player_ && player_->GetPresentationAlpha() > 0.001f;
	if (shouldDrawPlayer) {
		// 機体・ノズルは不透明パス。炎は地形の深度が完成してから描く。
		if (isEditorPreviewPlaying_ && player_->GetBoosterEffect()) {
			// 編集UIを閉じて再生した際に位置確認用の黄色い球を残さない。
			player_->GetBoosterEffect()->SetPlacementMarkersVisible(false);
		}
		player_->Draw(renderCamera, false);
	}

	if (missileManager_) {
		missileManager_->Draw();
	}


	if (enemyBulletManager_ && !(warpExitPresentation_.IsActive() || introBoostState_.IsActive())) {
		enemyBulletManager_->Draw(renderCamera);
	}

	Vector4 frustumPlanes[6];
	MyMath::ExtractFrustumPlanes(renderCamera->GetViewProjectionMatrix(), frustumPlanes);


	for (const auto &enemy : enemies_) {
		Sphere enemySphere;
		enemySphere.center = enemy->GetPosition();
		enemySphere.radius = enemy->GetCollisionRadius();


		if (MyMath::IsInFrustum(enemySphere, frustumPlanes)) {
			enemy->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}

	for (const NormalEnemyDebrisPiece& debris : normalEnemyDebris_) {
		if (!debris.object) {
			continue;
		}
		Sphere debrisSphere;
		debrisSphere.center = debris.object->GetTranslate();
		debrisSphere.radius = MyMath::Length(debris.object->GetScale());
		if (MyMath::IsInFrustum(debrisSphere, frustumPlanes)) {
			debris.object->Draw();
		}
	}
	for (const HomingEnemyImplosion& implosion : homingEnemyImplosions_) {
		if (!implosion.object) {
			continue;
		}
		Sphere implosionSphere;
		implosionSphere.center = implosion.object->GetTranslate();
		implosionSphere.radius = MyMath::Length(implosion.object->GetScale());
		if (MyMath::IsInFrustum(implosionSphere, frustumPlanes)) {
			implosion.object->Draw();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	for (const auto &obstacle : obstacles_) {
		Sphere obsSphere;
		obsSphere.center = obstacle->GetPosition();
		obsSphere.radius = MyMath::Length(obstacle->GetWorldHalfExtents());


		if (MyMath::IsInFrustum(obsSphere, frustumPlanes)) {
			obstacle->Draw();
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();


	if (showPlane) {
		for (Object3d* object3d : objects) {
			object3d->Draw();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();


	if (showModel && myModelObject) {
		myModelObject->Draw();
	}

	// ゴールリングは地面・障害物などの不透明モデルが深度を書き終えた後に描く。
	// 加算合成のリングを先に描くと、後続の地面描画で上書きされて奥に隠れてしまう。
	tutorialGoalMarker_.Draw(tutorialController_.GetPhaseFrames());

	// 地形・障害物を含む不透明物の深度を書き終えてから、敵の半透明部位を描く。
	// シールドは Z テストを行いながら Z 書き込みをしないため、この順序が必要になる。
	for (const auto& enemy : enemies_) {
		Sphere enemySphere;
		enemySphere.center = enemy->GetPosition();
		enemySphere.radius = enemy->GetCollisionRadius();
		if (MyMath::IsInFrustum(enemySphere, frustumPlanes)) {
			enemy->DrawTransparent();
		}
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();

	// 訓練中は敵も危険地帯も存在しないため、境界シールドを出さない。
	boundaryShieldRenderer_.Draw(!tutorialController_.IsEnabled(), player_.get(), boundaryShieldSettings_.Get());
	
	if (showBones) {

		Object3dCommon::GetInstance()->SetCommonDrawSettings();


		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
	}

	debugColliderRenderer_.Draw(showDebugColliders);
	

	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (environmentRenderer_) environmentRenderer_->Draw();
	warpExitPresentation_.DrawGate();

	// 歌の発動中だけ、ノズルではなく胴体・翼端が通った軌跡をリボンとして描く。
	// 曲線で残るため、旋回時にも機体の残像が空間へ描かれる。
	if (songState_.IsActive() && player_) {
		songFlightTrailRenderer_.Draw(renderCamera, *player_, songState_.GetFrame());
	}

	// すべての不透明オブジェクトの後に、実際の噴射を加算する。
	// Z-Test は有効、Z-Write は無効のまま、前景の翼・地形には正しく隠れる。
	if (shouldDrawPlayer) {
		player_->DrawEffects(renderCamera);
	}


	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (explosionManager_) explosionManager_->Draw();



	SpriteCommon::GetInstance()->SetCommonPipelineState();

	if (showSprite) {
		sprite->Draw();
	}

	DrawOverlay();
}

void GamePlayScene::SpawnNormalEnemyDebris(Enemy& enemy) {
	constexpr int kPieceCount = 5;
	constexpr int kLifetimeFrames = 52;

	const Vector3 center = enemy.GetAimPosition();
	const Vector3 halfExtents = enemy.GetWorldHalfExtents();
	if (environmentRenderer_) {
		if (ParticleManager* particleManager = environmentRenderer_->GetParticleManager()) {
			// 実弾で撃ち砕いた瞬間だけ、白芯を含む短命の橙色スパークを出す。
			particleManager->Emit("test", center, 42, { 1.0f, 0.42f, 0.06f, 1.0f },
				1.30f, 0.55f, 0.085f, 0.040f, 0.24f, 0.46f, 0.10f);
		}
	}
	for (int index = 0; index < kPieceCount; ++index) {
		const float phase = static_cast<float>(index) * 2.39996323f;
		const float radialDistance = 0.36f + static_cast<float>(index % 3) * 0.16f;
		const float sizeVariation = 0.26f + static_cast<float>((index * 5) % 3) * 0.06f;

		NormalEnemyDebrisPiece debris;
		debris.object = std::make_unique<Object3d>();
		debris.object->Initialize(Object3dCommon::GetInstance());
		debris.object->SetModel("NormalEnemyDebris");
		debris.baseScale = {
			(std::max)(0.18f, halfExtents.x * sizeVariation),
			(std::max)(0.12f, halfExtents.y * (sizeVariation * 0.72f)),
			(std::max)(0.18f, halfExtents.z * (sizeVariation * 1.25f))
		};
		debris.rotation = { phase * 0.37f, phase, phase * 0.61f };
		debris.angularVelocity = {
			0.12f + static_cast<float>(index % 3) * 0.035f,
			0.16f + static_cast<float>(index % 5) * 0.025f,
			0.10f + static_cast<float>(index % 4) * 0.030f
		};
		debris.velocity = {
			std::cos(phase) * (0.28f + radialDistance * 0.48f),
			0.20f + static_cast<float>(index % 3) * 0.060f,
			std::sin(phase) * (0.28f + radialDistance * 0.48f)
		};
		debris.remainingFrames = kLifetimeFrames;

		debris.object->SetTranslate({
			center.x + std::cos(phase) * halfExtents.x * radialDistance,
			center.y + std::sin(phase * 1.7f) * halfExtents.y * radialDistance,
			center.z + std::sin(phase) * halfExtents.z * radialDistance
		});
		debris.object->SetScale(debris.baseScale);
		debris.object->SetRotate(debris.rotation);
		debris.object->Update();
		normalEnemyDebris_.push_back(std::move(debris));
	}
	// 本体は同フレームから描画しない。次フレームの通常撃破処理でリストからも削除される。
	enemy.SetPresentationVisible(false);
}

void GamePlayScene::UpdateNormalEnemyDebris() {
	constexpr int kFadeFrames = 14;
	for (auto it = normalEnemyDebris_.begin(); it != normalEnemyDebris_.end();) {
		NormalEnemyDebrisPiece& debris = *it;
		if (!debris.object || --debris.remainingFrames <= 0) {
			it = normalEnemyDebris_.erase(it);
			continue;
		}

		debris.velocity.y -= 0.010f;
		Vector3 position = debris.object->GetTranslate();
		position.x += debris.velocity.x;
		position.y += debris.velocity.y;
		position.z += debris.velocity.z;
		debris.rotation.x += debris.angularVelocity.x;
		debris.rotation.y += debris.angularVelocity.y;
		debris.rotation.z += debris.angularVelocity.z;

		const float visibleScale = (std::min)(1.0f,
			static_cast<float>(debris.remainingFrames) / static_cast<float>(kFadeFrames));
		debris.object->SetTranslate(position);
		debris.object->SetRotate(debris.rotation);
		debris.object->SetScale({
			debris.baseScale.x * visibleScale,
			debris.baseScale.y * visibleScale,
			debris.baseScale.z * visibleScale
		});
		debris.object->Update();
		++it;
	}
}

void GamePlayScene::SpawnHomingEnemyImplosion(Enemy& enemy) {
	Model* model = enemy.GetPresentationModel();
	if (!model) {
		return;
	}

	HomingEnemyImplosion implosion;
	implosion.object = std::make_unique<Object3d>();
	implosion.object->Initialize(Object3dCommon::GetInstance());
	implosion.object->SetModelPointer(model);
	implosion.baseScale = enemy.GetScale();
	implosion.rotation = enemy.GetRotation();
	// The ring animation uses the same 30-frame duration in ExplosionManager.
	implosion.remainingFrames = 30;
	implosion.object->SetTranslate(enemy.GetAimPosition());
	implosion.object->SetScale(implosion.baseScale);
	implosion.object->SetRotate(implosion.rotation);
	implosion.object->Update();
	homingEnemyImplosions_.push_back(std::move(implosion));
	enemy.SetPresentationVisible(false);
}

void GamePlayScene::UpdateHomingEnemyImplosions() {
	constexpr int kDurationFrames = 30;
	for (auto it = homingEnemyImplosions_.begin(); it != homingEnemyImplosions_.end();) {
		HomingEnemyImplosion& implosion = *it;
		if (!implosion.object || --implosion.remainingFrames <= 0) {
			if (implosion.object && environmentRenderer_) {
				if (ParticleManager* particleManager = environmentRenderer_->GetParticleManager()) {
					// 収束直後に白い高熱フラッシュを一度だけ広げて消す。
					particleManager->EmitFireball("test", implosion.object->GetTranslate(), 1,
						{ 1.0f, 1.0f, 1.0f, 0.90f }, 0.50f, 4.0f, 0.15f, 0.0f);
				}
			}
			it = homingEnemyImplosions_.erase(it);
			continue;
		}

		const float progress = 1.0f - static_cast<float>(implosion.remainingFrames) /
			static_cast<float>(kDurationFrames);
		const float scale = (1.0f - progress) * (1.0f - progress);
		implosion.rotation.x += 0.055f;
		implosion.rotation.y += 0.180f;
		implosion.rotation.z += 0.090f;
		implosion.object->SetScale({
			implosion.baseScale.x * scale,
			implosion.baseScale.y * scale,
			implosion.baseScale.z * scale
		});
		implosion.object->SetRotate(implosion.rotation);
		implosion.object->Update();
		++it;
	}
}

void GamePlayScene::ClearNormalEnemyDebris() {
	normalEnemyDebris_.clear();
	homingEnemyImplosions_.clear();
}

void GamePlayScene::BeginTutorialCombat() {
	if (tutorialController_.HasCombatTargets() || !player_) {
		return;
	}

	// 本編のスポーン情報をチュートリアルへ持ち込まない。ここで生成する3機だけが
	// 攻撃フェーズの対象になり、プレイヤーを追尾・攻撃しない。
	ClearAllEnemyReferences();
	enemies_.clear();
	enemySpawns_.clear();
	enemySpawnManager_.Clear();
	bossEncounterState_.Reset();

	const Vector3 playerPosition = player_->GetPosition();
	const Vector3 forward = NormalizeOrVector3(player_->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	const Vector3 right = NormalizeOrVector3({ forward.z, 0.0f, -forward.x }, { 1.0f, 0.0f, 0.0f });
	const std::array<Vector3, 3> targetOffsets = {
		AddVector3(ScaleVector3(forward, 22.0f), { 0.0f, 1.5f, 0.0f }),
		AddVector3(AddVector3(ScaleVector3(forward, 27.0f), ScaleVector3(right, -7.0f)), { 0.0f, 4.5f, 0.0f }),
		AddVector3(AddVector3(ScaleVector3(forward, 31.0f), ScaleVector3(right, 7.0f)), { 0.0f, -1.5f, 0.0f }),
	};

	for (const Vector3& offset : targetOffsets) {
		auto target = std::make_unique<Enemy>();
		target->Initialize(AddVector3(playerPosition, offset));
		target->SetTrainingTarget(true);
		enemies_.push_back(std::move(target));
	}
	tutorialController_.MarkCombatTargetsSpawned();
}

void GamePlayScene::ActivateTutorialGoal() {
	tutorialGoalMarker_.Activate(player_.get(), obstacles_);
}

void GamePlayScene::UpdateTutorial() {
	if (!tutorialController_.IsEnabled() || tutorialController_.GetPhase() == TutorialPhase::Disabled ||
		bossClearSequence_.IsActive() ||
		IsTitleBackgroundMode() || IsAttractDemoMode() || pauseMenuState_.IsPaused() || gameOverSequence_.IsActive() ||
		warpExitPresentation_.IsActive() || introBoostState_.IsActive()) {
		return;
	}

	Input* input = Input::GetInstance();
	if (!input) {
		return;
	}
	TutorialPhaseInput tutorialInput;
	tutorialInput.skipRequested = input->TriggerKey(DIK_F8);
	tutorialInput.forwardTriggered = input->TriggerAction(PlayerAction::MoveForward);
	// マウスだけでなく、コントローラーのピッチ操作でも飛行訓練を達成できるようにする。
	tutorialInput.altitudeInput = input->GetMouseDeltaY() != 0 ||
		input->PushAction(PlayerAction::PitchUp) || input->PushAction(PlayerAction::PitchDown);
	tutorialInput.turnTriggered = input->TriggerAction(PlayerAction::DodgeLeft) || input->TriggerAction(PlayerAction::DodgeRight);
	tutorialInput.isGerwalk = player_ && player_->GetCurrentMode() == PlayerMode::Gerwalk;
	tutorialInput.gerwalkHoverTriggered = input->TriggerAction(PlayerAction::MoveUp) || input->TriggerAction(PlayerAction::MoveDown);
	tutorialInput.gerwalkStrafeTriggered = tutorialInput.turnTriggered;
	tutorialInput.isLocked = targetSelection_.HasAliveLockedEnemy();
	tutorialInput.isFighter = player_ && player_->GetCurrentMode() == PlayerMode::Fighter;
	tutorialInput.combatTargetsDefeated = enemies_.empty();
	tutorialInput.goalReached = tutorialGoalMarker_.IsActive() && player_ &&
		MyMath::Length(SubtractVector3(player_->GetPosition(), tutorialGoalMarker_.GetPosition())) <= 3.5f;
	tutorialInput.completeConfirmed = input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) ||
		input->TriggerControllerButton(XINPUT_GAMEPAD_A) || input->TriggerControllerButton(XINPUT_GAMEPAD_START);

	const TutorialPhaseResult result = tutorialController_.Update(tutorialInput, tutorialRequirements_.Get(), tutorialProgress_);
	if (result.clearGoal) {
		tutorialGoalMarker_.Deactivate();
	}
	if (result.beginCombat) {
		BeginTutorialCombat();
	}
	if (result.activateGoal) {
		ActivateTutorialGoal();
	}
	if (result.clearMission && player_) {
		MissionResultStore::Set({ MissionResultOutcome::Clear, defeatedEnemyCount_, player_->GetHP(), gameplayElapsedFrames_,
			false, MissionStage::Tutorial });
		SceneManager::GetInstance()->ChangeScene("CLEAR");
	}
}

void GamePlayScene::DrawOverlay() {
	if (isDebugCameraActive_ && !debugFlyCamera_) return;
	if (!isDebugCameraActive_ && !camera) return;

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (!activeCamera) return;

	// 出撃カット中は開始演出だけを重ね、戦闘 HUD・レーダー・照準を一切出さない。
	if (warpExitPresentation_.IsActive() || introBoostState_.IsActive()) {
		missionStartOverlayRenderer_.Draw(missionStartOverlaySprite_.get(), warpExitPresentation_);
		if (warpFadeInSprite_ && warpFadeInAlpha_ > 0.001f) {
			const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
			const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
			warpFadeInSprite_->SetPosition({ 0.0f, 0.0f });
			warpFadeInSprite_->SetSize({ screenWidth, screenHeight });
			warpFadeInSprite_->SetColor({ 1.0f, 1.0f, 1.0f, warpFadeInAlpha_ });
			warpFadeInSprite_->Update();
			warpFadeInSprite_->Draw();
		}
		return;
	}

	if (IsTitleBackgroundMode()) {
		// タイトルでは戦闘 HUD を出さず、ロックオン表示だけを残す。
		reticleOverlayRenderer_.DrawTitleBackgroundLockOn(targetSelection_.GetLockedEnemy(), activeCamera);
		return;
	}

	// タイトルへの帰還演出が始まった瞬間から、ゲームプレイ用の HUD・レーダー・
	// 照準・チュートリアルを一切描かない。終了演出だけを最前面に重ねることで、
	// フェード中に一部の UI が残る現象を防ぐ。
	if (returnToTitleTransition_.IsActive()) {
		returnToTitleOverlayRenderer_.Draw(returnToTitleTransition_, pauseOverlaySprite_.get(), pausePanelSprite_.get(),
			static_cast<float>(WinApp::GetClientWidth()), static_cast<float>(WinApp::GetClientHeight()));
		return;
	}

	// HUD
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	// 撃破演出中は通常 HUD を隠し、ボス本体と専用メッセージだけを見せる。
	if (bossClearSequence_.IsActive()) {
		bossClearOverlayRenderer_.Draw(
			bossClearSequence_.GetPhaseState(), bossClearSequence_.GetTimeline(), bossClearSequence_.GetEffectState(),
			BossClearSequenceController::kPhase2FlashDurationSec, BossClearSequenceController::kMissionClearDurationSec,
			bossCinematicTopBarSprite_.get(), bossCinematicBottomBarSprite_.get(),
			gameOverWhiteoutSprite_.get(), missionClearOverlaySprite_.get(),
			screenWidth, screenHeight);
		return;
	}
	const CombatHudLayout hudLayout = combatHudSpriteRenderer_.DrawBaseHud({
		screenWidth,
		screenHeight,
		gameplayBalanceSettings_.Get().songDurationFrames,
		player_.get(),
		&enemies_,
		ammoManager_.get(),
		&songState_,
		&specialAttackState_,
	});

	combatHudOverlayRenderer_.Draw({
		screenWidth,
		screenHeight,
		hudLayout.statusPanelX,
		hudLayout.statusPanelY,
		hudLayout.statusPanelWidth,
		hudLayout.statusPanelHeight,
		hudLayout.ammoPanelX,
		hudLayout.normalAmmoPanelY,
		hudLayout.homingAmmoPanelY,
		hudLayout.normalAmmoTextY,
		hudLayout.homingAmmoTextY,
		hudLayout.gaugeX,
		hudLayout.songGaugeY,
		hudLayout.songGaugeWidth,
		tutorialController_.IsEnabled(),
		pauseMenuState_.IsPaused(),
		gameOverSequence_.IsActive(),
		gameplayElapsedFrames_,
		player_.get(),
		activeCamera,
		ammoManager_.get(),
		&songState_,
		&specialAttackState_,
		&combatAlertState_,
	});
#ifdef CG2_ENABLE_GAME_UI
	const bool showStageObjective = !IsTitleBackgroundMode() && !IsAttractDemoMode() &&
		!pauseMenuState_.IsPaused() && !gameOverSequence_.IsActive();
	stageObjectiveOverlayRenderer_.Draw(showStageObjective, player_.get(), enemies_,
		bossEncounterState_.IsBossSpawned(), screenWidth, screenHeight);
#endif

	tutorialOverlayRenderer_.Draw(tutorialController_, tutorialProgress_, tutorialRequirements_.Get(), tutorialGoalMarker_,
		player_.get(), activeCamera, enemies_.size(), screenWidth, screenHeight);
	radarRenderer_.Draw(player_.get(), enemies_, targetSelection_.GetLockedEnemy(), multiLockChargeState_, missileManager_.get());

	const bool isJammed = lockOnManager_->IsPlayerJammed(activeCamera);
	reticleOverlayRenderer_.DrawLockAndAimCursor(
		player_.get(), targetSelection_.GetLockedEnemy(), targetSelection_.GetAimAssistEnemy(), lockOnManager_.get(), activeCamera, isJammed);
	combatHudSpriteRenderer_.DrawLockedTargetOverheadHp(
		targetSelection_.GetLockedEnemy(), lockOnManager_.get(), activeCamera, screenWidth, screenHeight);
	reticleOverlayRenderer_.DrawMultiLockMarkers(
		multiLockChargeState_, missileManager_.get(), activeCamera, isJammed);
	bossIntroOverlayRenderer_.Draw(bossEncounterState_, screenWidth, screenHeight);
	DrawPauseOverlay(screenWidth, screenHeight);

	// ワープ終了時のホワイトアウトからゲームステージへのフェードイン
	if (warpFadeInSprite_ && warpFadeInAlpha_ > 0.001f) {
		warpFadeInSprite_->SetPosition({ 0.0f, 0.0f });
		warpFadeInSprite_->SetSize({ screenWidth, screenHeight });
		warpFadeInSprite_->SetColor({ 1.0f, 1.0f, 1.0f, warpFadeInAlpha_ });
		warpFadeInSprite_->Update();
		warpFadeInSprite_->Draw();
	}

	// 機体の爆発中だけ中央に失敗通知を出し、リザルトへ移る前に確実に伝える。
	// ホワイトアウトの進行と同じ比率で消えるため、次の画面へ文字が残らない。
	gameOverOverlayRenderer_.Draw(gameOverSequence_, screenWidth, screenHeight);
	attractDemoOverlayRenderer_.Draw(
		IsAttractDemoMode() && !gameOverSequence_.IsActive(),
		attractDemoController_.GetState(), screenWidth, screenHeight);

	if (gameOverSequence_.IsActive() && gameOverWhiteoutSprite_) {
		const float whiteoutProgress = SmoothStep01(
			(static_cast<float>(gameOverSequence_.GetFrame()) - 96.0f) /
			(static_cast<float>(GameOverSequence::kCinematicFrames) - 96.0f));
		gameOverWhiteoutSprite_->SetPosition({ 0.0f, 0.0f });
		gameOverWhiteoutSprite_->SetSize({ screenWidth, screenHeight });
		gameOverWhiteoutSprite_->SetColor({ 1.0f, 1.0f, 1.0f, whiteoutProgress });
		gameOverWhiteoutSprite_->Update();
		gameOverWhiteoutSprite_->Draw();
	}
}

void GamePlayScene::DrawPauseOverlay(float screenWidth, float screenHeight) {
	if (returnToTitleTransition_.IsActive()) {
		returnToTitleOverlayRenderer_.Draw(returnToTitleTransition_, pauseOverlaySprite_.get(), pausePanelSprite_.get(),
			screenWidth, screenHeight);
		return;
	}

	pauseMenuOverlayRenderer_.Draw(pauseMenuState_, pauseOverlaySprite_.get(), pausePanelSprite_.get(),
		pauseMenuLabelsSprite_.get(), screenWidth, screenHeight);
}

void GamePlayScene::StartReturnToTitleTransition() {
	if (!returnToTitleTransition_.Start()) {
		return;
	}

	// 通信切断: BGMやボイスの音量を即座に落として通信途絶感を出す
	if (pVoice1) {
		pVoice1->SetVolume(0.0f);
	}
	if (pVoice2) {
		pVoice2->SetVolume(0.0f);
	}
	if (pSongVoice) {
		pSongVoice->SetVolume(0.0f);
	}

	// ポストエフェクトを即座に通信切断・シャットダウン初期状態（ノイズ・走査線）に切り替え
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetSystemShutdown(0.0f, 1.0f);
	}
}

void GamePlayScene::UpdateReturnToTitleTransition() {
	const bool transitionFinished = returnToTitleTransition_.Advance();
	const float progress = returnToTitleTransition_.GetProgress();

	float glitchIntensity = 1.0f;
	if (progress > 0.85f) {
		glitchIntensity = std::clamp((1.0f - progress) / 0.15f, 0.0f, 1.0f);
	}

	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetSystemShutdown(progress, glitchIntensity);
	}

	if (transitionFinished) {
		// 完全暗転到達 -> ポストエフェクトを戻し、タイトル画面へ遷移
		if (PostEffect::GetInstance()) {
			PostEffect::GetInstance()->SetEffectType(0);
		}
		TitleScene::SetPendingFadeIn(true);
		SceneManager::GetInstance()->ChangeScene("TITLE");
		return;
	}
}
