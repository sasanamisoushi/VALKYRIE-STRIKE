#include "GamePlayScene.h"
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
#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <shellapi.h>

using json = nlohmann::json;

namespace {
	constexpr int kNoEnemyRespawnTimer = -1;
	constexpr float kSpecialAttackCost = 50.0f;
	constexpr float kSpGaugeRecoveryPerFrame = 3.0f / 60.0f;
	constexpr int kSpecialAttackDurationFrames = 180;
	constexpr int kSpecialAttackFireIntervalFrames = 6;
	constexpr int kBossMaxActiveMinions = 6;
}




GamePlayScene::GamePlayScene(Mode mode)
	: mode_(mode) {
}

void GamePlayScene::BeginTitleLaunch() {
	if (!IsTitleBackgroundMode() || !player_ || titleLaunchActive_) {
		return;
	}

	titleLaunchActive_ = true;
	titleLaunchFrame_ = 0;
	titleLaunchStartPosition_ = player_->GetPosition();
	titleLaunchStartForward_ = NormalizeOrVector3(player_->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	titleLaunchStartForward_.y = 0.0f;
	titleLaunchStartForward_ = NormalizeOrVector3(titleLaunchStartForward_, { 0.0f, 0.0f, 1.0f });
	// タイトルで飛んでいた向きを、そのまま本編開始時へ渡す。
	titleLaunchForward_ = titleLaunchStartForward_;
}

bool GamePlayScene::TryConsumeAmmo(MissileType type) {
	return ammoManager_ && ammoManager_->TryConsume(type);
}

void GamePlayScene::UpdateAutoReload() {
	if (ammoManager_) ammoManager_->UpdateAutoReload();
}

void GamePlayScene::ClearEnemyReferences(Enemy* enemy) {
	if (!enemy) {
		return;
	}

	if (lockedEnemy_ == enemy) {
		lockedEnemy_ = nullptr;
		isCinematicLockOnCameraInitialized_ = false;
	}
	if (aimAssistEnemy_ == enemy) {
		aimAssistEnemy_ = nullptr;
	}
	multiLockTargets_.erase(
		std::remove(multiLockTargets_.begin(), multiLockTargets_.end(), enemy),
		multiLockTargets_.end());
	if (missileManager_) {
		missileManager_->ClearTarget(enemy);
	}
}

void GamePlayScene::ClearAllEnemyReferences() {
	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	multiLockTargets_.clear();
	isCinematicLockOnCameraInitialized_ = false;
	if (missileManager_) {
		missileManager_->ClearAllTargets();
	}
}

void GamePlayScene::Initialize() {


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
	TextureManager::GetInstance()->LoadTexture(kLockOnReticleTexturePath);
	TextureManager::GetInstance()->LoadTexture(kMissileLockOnReticleTexturePath);
	TextureManager::GetInstance()->LoadTexture(kAimCursorTexturePath);
	TextureManager::GetInstance()->LoadTexture(kBoundaryAlertTexturePath);

	aimCursorSprite_ = std::make_unique<Sprite>();
	aimCursorSprite_->Initialize(SpriteCommon::GetInstance(), kAimCursorTexturePath);
	aimCursorSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	lockOnReticleSprite_ = std::make_unique<Sprite>();
	lockOnReticleSprite_->Initialize(SpriteCommon::GetInstance(), kLockOnReticleTexturePath);
	lockOnReticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	missileLockOnReticleSprite_ = std::make_unique<Sprite>();
	missileLockOnReticleSprite_->Initialize(SpriteCommon::GetInstance(), kMissileLockOnReticleTexturePath);
	missileLockOnReticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	TextureManager::GetInstance()->LoadTexture("resources/multi_lock_marker.png");
	for (auto& markerSprite : multiLockMarkerSprites_) {
		markerSprite = std::make_unique<Sprite>();
		markerSprite->Initialize(SpriteCommon::GetInstance(), "resources/multi_lock_marker.png");
		markerSprite->SetAnchorPoint({ 0.5f, 0.5f });
	}

	TextureManager::GetInstance()->LoadTexture("resources/white1x1.png");
	spGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	spGaugeBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	spGaugeFillSprite_ = std::make_unique<Sprite>();
	spGaugeFillSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	spGaugeCostMarkerSprite_ = std::make_unique<Sprite>();
	spGaugeCostMarkerSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	songGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	songGaugeBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	songGaugeFillSprite_ = std::make_unique<Sprite>();
	songGaugeFillSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	songGaugeStateSprite_ = std::make_unique<Sprite>();
	songGaugeStateSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");

	const char *hudTexturePaths[] = {
		"resources/hud_panel_frame.png",
		"resources/hud_label_hp.png",
		"resources/hud_label_ammo.png",
		"resources/hud_label_sp.png",
		"resources/hud_digits.png",
		"resources/hud_ammo_icons.png"
	};
	for (const char *path : hudTexturePaths) {
		TextureManager::GetInstance()->LoadTexture(path);
	}
	TextureManager::GetInstance()->LoadTexture("resources/gameplay_controls.png");
	TextureManager::GetInstance()->LoadTexture("resources/pause_menu_labels.png");
	hudPanelSprite_ = std::make_unique<Sprite>();
	hudPanelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[0]);
	hudAmmoPanelSprite_ = std::make_unique<Sprite>();
	hudAmmoPanelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[0]);
	hpGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	hpGaugeBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	hpGaugeFillSprite_ = std::make_unique<Sprite>();
	hpGaugeFillSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	bossHpGaugeBackgroundSprite_ = std::make_unique<Sprite>();
	bossHpGaugeBackgroundSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	bossHpGaugeFillSprite_ = std::make_unique<Sprite>();
	bossHpGaugeFillSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	pauseOverlaySprite_ = std::make_unique<Sprite>();
	pauseOverlaySprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	pausePanelSprite_ = std::make_unique<Sprite>();
	pausePanelSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	controlsGuideSprite_ = std::make_unique<Sprite>();
	controlsGuideSprite_->Initialize(SpriteCommon::GetInstance(), "resources/gameplay_controls.png");
	pauseMenuLabelsSprite_ = std::make_unique<Sprite>();
	pauseMenuLabelsSprite_->Initialize(SpriteCommon::GetInstance(), "resources/pause_menu_labels.png");
	hudHpLabelSprite_ = std::make_unique<Sprite>();
	hudHpLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[1]);
	hudAmmoLabelSprite_ = std::make_unique<Sprite>();
	hudAmmoLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[2]);
	hudSpLabelSprite_ = std::make_unique<Sprite>();
	hudSpLabelSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[3]);
	hudNormalAmmoIconSprite_ = std::make_unique<Sprite>();
	hudNormalAmmoIconSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[5]);
	hudHomingAmmoIconSprite_ = std::make_unique<Sprite>();
	hudHomingAmmoIconSprite_->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[5]);
	hudNormalReloadGaugeSprite_ = std::make_unique<Sprite>();
	hudNormalReloadGaugeSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	hudHomingReloadGaugeSprite_ = std::make_unique<Sprite>();
	hudHomingReloadGaugeSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("resources/radar_frame.png");
	radarFrameSprite_ = std::make_unique<Sprite>();
	radarFrameSprite_->Initialize(SpriteCommon::GetInstance(), "resources/radar_frame.png");
	radarFrameSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	radarSweepSprite_ = std::make_unique<Sprite>();
	radarSweepSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	radarSweepSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	for (auto &blipSprite : radarBlipSprites_) {
		blipSprite = std::make_unique<Sprite>();
		blipSprite->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
		blipSprite->SetAnchorPoint({ 0.5f, 0.5f });
	}
	for (auto &digitSprite : hudHpDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	for (auto &digitSprite : hudNormalAmmoDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	for (auto &digitSprite : hudHomingAmmoDigitSprites_) {
		digitSprite = std::make_unique<Sprite>();
		digitSprite->Initialize(SpriteCommon::GetInstance(), hudTexturePaths[4]);
	}
	spGauge_ = 100.0f;
	isSpecialAttackActive_ = false;
	specialAttackFrame_ = 0;
	songGauge_ = 100.0f;
	isSongActive_ = false;
	songFrame_ = 0;
	ammoManager_->Initialize();

	ModelManager::GetInstance()->CreatePlaneModel("BoundaryAlertPlane");
	Model* alertModel = ModelManager::GetInstance()->FindModel("BoundaryAlertPlane");
	if (alertModel) {
		alertModel->SetTextureFilePath(kBoundaryAlertTexturePath);
		alertModel->SetAlphaReference(0.05f); // Discard almost-black background
	}
	boundaryAlertObject_ = std::make_unique<Object3d>();
	boundaryAlertObject_->Initialize(Object3dCommon::GetInstance());
	boundaryAlertObject_->SetModel("BoundaryAlertPlane");
	ceilingBoundaryAlertObject_ = std::make_unique<Object3d>();
	ceilingBoundaryAlertObject_->Initialize(Object3dCommon::GetInstance());
	ceilingBoundaryAlertObject_->SetModel("BoundaryAlertPlane");


	// SkyboxCommon is now initialized in Framework.cpp





	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("multiMesh.obj");
	ModelManager::GetInstance()->CreateSphereModel("Sphere", 16);
	ModelManager::GetInstance()->CreateSphereModel("EnemyBulletSphere", 6);

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


	ModelManager::GetInstance()->CreateLineModel("DebugColliderLines");
	debugColliderLinesObject = std::make_unique<Object3d>();
	debugColliderLinesObject->Initialize(Object3dCommon::GetInstance());
	debugColliderLinesObject->SetModel("DebugColliderLines");


	debugFlyCamera_ = std::make_unique<FlyCamera>();
	debugFlyCamera_->SetTranslate({ 0.0f, 5.0f, -20.0f });
	isDebugCameraActive_ = false;









	environmentRenderer_->GetParticleManager()->CreateParticleGroup("test", "resources/circle.png");
	environmentRenderer_->GetParticleManager()->CreateParticleGroup("smoke", "resources/circle.png");


	soundData1 = AudioManager::GetInstance()->LoadWave("resources/Alarm01.wav");
	soundData2 = AudioManager::GetInstance()->LoadAudio("resources/maou_bgm_fantasy15.mp3");
	songSoundData = AudioManager::GetInstance()->LoadAudio("resources/song_bgm.mp3");

	// タイトル背景では、タイトル側の音と競合しないよう本編BGMを鳴らさない。
	if (!IsTitleBackgroundMode()) {
		pVoice1 = AudioManager::GetInstance()->PlayWave(soundData1, true);
		pVoice2 = AudioManager::GetInstance()->PlayWave(soundData2, true);
		pSongVoice = AudioManager::GetInstance()->PlayWave(songSoundData, true);
		if (pSongVoice) pSongVoice->SetVolume(0.0f);
	}


	ModelManager::GetInstance()->CreateTrailModel("SmokeTrail");


	missileTrail = std::make_unique<Trail>();
	missileTrail->Initialize(60);


	trailObject = std::make_unique<Object3d>();
	trailObject->Initialize(Object3dCommon::GetInstance());
	trailObject->SetModel("SmokeTrail");


	ModelManager::GetInstance()->CreateBoxModel("PlayerBox");
	ModelManager::GetInstance()->CreateBoxModel("EnemyBox");
	ModelManager::GetInstance()->CreateBoxModel("BossHull");
	ModelManager::GetInstance()->CreateBoxModel("BossCannon");
	ModelManager::GetInstance()->CreateBoxModel("BossBeam");
	if (Model *model = ModelManager::GetInstance()->FindModel("BossHull")) model->SetColor({ 0.18f, 0.22f, 0.35f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("BossCannon")) model->SetColor({ 1.0f, 0.35f, 0.05f, 1.0f });
	if (Model *model = ModelManager::GetInstance()->FindModel("BossBeam")) model->SetColor({ 0.15f, 0.8f, 1.0f, 1.0f });
	ModelManager::GetInstance()->CreateBoxModel("ObstacleBox");

	player_ = std::make_unique<Player>();
	player_->Initialize(kPlayerModelName);


	missileManager_ = std::make_unique<MissileManager>();
	missileManager_->Initialize(environmentRenderer_->GetParticleManager());


	explosionManager_ = std::make_unique<ExplosionManager>();
	explosionManager_->Initialize(environmentRenderer_->GetParticleManager());

	enemies_.clear();

	enemyBulletManager_ = std::make_unique<EnemyBulletManager>();
	enemyBulletManager_->Initialize();

	simulationManager_ = std::make_unique<SimulationManager>(this);
	missilePresetManager_ = std::make_unique<MissilePresetManager>(this);
	lockOnManager_ = std::make_unique<LockOnManager>(this);


	isGameOver_ = false;
	gameOverTimer_ = 0;
	isPaused_ = false;
	isPauseTitleSelected_ = false;
	// 通常のゲーム起動では、編集ツールの再生ボタンを押さなくても最初から操作できる。
	// シミュレーション専用起動だけは下で停止状態にする。
	isEditorPreviewPlaying_ = !IsSimulationMode();

	ReloadSceneJson();
	if (!IsSimulationMode() && !IsTitleBackgroundMode()) {
		GameStartTransitionData titleTransition;
		if (GameStartTransition::Consume(titleTransition) && player_) {
			const float horizontalLength = std::sqrt(
				titleTransition.forward.x * titleTransition.forward.x +
				titleTransition.forward.z * titleTransition.forward.z);
			if (horizontalLength > 0.0001f) {
				const float yaw = std::atan2(titleTransition.forward.x, titleTransition.forward.z);
				player_->SetRotation({ 0.0f, yaw, 0.0f });
			}
			introBoostVelocity_ = titleTransition.forward;
			introBoostVelocity_.x *= titleTransition.initialSpeed;
			introBoostVelocity_.y *= titleTransition.initialSpeed;
			introBoostVelocity_.z *= titleTransition.initialSpeed;
			introBoostFrames_ = titleTransition.boostFrames;
			introBoostInitialFrames_ = titleTransition.boostFrames;
			introBoostYaw_ = std::atan2(titleTransition.forward.x, titleTransition.forward.z);
		}
	}
// 	simulationManager_->RefreshSimulationActionNames();
// 	missilePresetManager_->RefreshMissilePresetNames();

	if (IsSimulationMode()) {
		isEditorPreviewPlaying_ = false;
		uiManager_->currentSimulationTarget_ = 2;
		uiManager_->showSimulationWindow_ = true;
		SetDebugCameraActive(true);
	}


	EditorReceiver::GetInstance()->Initialize();
}

void GamePlayScene::SetDebugCameraActive(bool isActive) {
	if (isDebugCameraActive_ == isActive) {
		return;
	}

	isDebugCameraActive_ = isActive;
	isCinematicLockOnCameraInitialized_ = false;
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
	if (!debugFlyCamera_) {
		return;
	}

	Vector3 target = camera ? camera->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
	Vector3 forward = { 0.0f, 0.0f, 1.0f };
	if (player_) {
		target = player_->GetPosition();
		forward = NormalizeOrVector3(player_->GetForwardVector(), forward);
	}

	const Vector3 position = {
		target.x - forward.x * 20.0f,
		target.y - forward.y * 20.0f + 7.0f,
		target.z - forward.z * 20.0f,
	};
	debugFlyCamera_->SetTranslate(position);
	debugFlyCamera_->SetQuaternion(MakeLookQuaternion(SubtractVector3(target, position)));
	debugFlyCamera_->Camera::Update();
}

void GamePlayScene::ResetDebugCameraToStageOverview() {
	if (!debugFlyCamera_) {
		return;
	}

	// StageBounds はステージ全体を囲むため、ここを基準にすると
	// ステージの大きさが変わってもフリーカメラの初期位置が追従する。
	const Obstacle* stageBounds = nullptr;
	for (const auto& obstacle : obstacles_) {
		if (obstacle && obstacle->IsStageBounds()) {
			stageBounds = obstacle.get();
			break;
		}
	}

	if (!stageBounds) {
		ResetDebugCameraToPlayer();
		return;
	}

	const Vector3 target = stageBounds->GetPosition();
	const Vector3 halfExtents = stageBounds->GetWorldHalfExtents();
	const float stageRadius = (std::max)(halfExtents.x, halfExtents.z);
	const float distance = (std::max)(stageRadius * 2.2f, 80.0f);
	const Vector3 position = {
		target.x,
		target.y + distance * 0.85f,
		target.z - distance * 1.10f,
	};

	debugFlyCamera_->SetFovY(0.90f);
	debugFlyCamera_->SetFarClip((std::max)(distance * 4.0f, 3000.0f));
	debugFlyCamera_->SetTranslate(position);
	debugFlyCamera_->SetQuaternion(MakeLookQuaternion(SubtractVector3(target, position)));
	debugFlyCamera_->Camera::Update();
}

void GamePlayScene::TryPlaceEnemyAtGameViewMouse() {
#ifdef ENABLE_IMGUI
	if (!uiManager_ || !uiManager_->enemyMousePlacementEnabled_ || !debugFlyCamera_ ||
		!simulationManager_ || !ImGuiManager::IsVisible() || ImGui::GetCurrentContext() == nullptr ||
		!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}

	Vector2 localMousePosition;
	if (!FlyCamera::GetGameViewMousePos(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y, localMousePosition)) {
		return;
	}

	float gameViewWidth = 0.0f;
	float gameViewHeight = 0.0f;
	FlyCamera::GetGameViewSize(gameViewWidth, gameViewHeight);
	if (gameViewWidth <= 0.0f || gameViewHeight <= 0.0f) {
		return;
	}

	const Ray ray = MyMath::ScreenToRay(
		localMousePosition,
		gameViewWidth,
		gameViewHeight,
		MyMath::Inverse(debugFlyCamera_->GetViewProjectionMatrix()));
	if (std::abs(ray.direction.y) <= 0.0001f) {
		uiManager_->simulationSaveMessage_ = "配置面と視線が平行のため、敵を配置できませんでした";
		return;
	}

	const float distance = (uiManager_->enemyMousePlacementHeight_ - ray.origin.y) / ray.direction.y;
	if (distance <= 0.0f) {
		uiManager_->simulationSaveMessage_ = "クリック位置がカメラの後方にあるため、敵を配置できませんでした";
		return;
	}

	const Vector3 position = {
		ray.origin.x + ray.direction.x * distance,
		uiManager_->enemyMousePlacementHeight_,
		ray.origin.z + ray.direction.z * distance,
	};
	const float degreesToRadians = 3.141592654f / 180.0f;
	const Vector3 rotation = {
		uiManager_->blenderEnemyRotationDegrees_[0] * degreesToRadians,
		uiManager_->blenderEnemyRotationDegrees_[1] * degreesToRadians,
		uiManager_->blenderEnemyRotationDegrees_[2] * degreesToRadians,
	};
	static constexpr const char *enemyTypes[] = { "VF1", "VF1-1", "VF3", "Jammer", "Boss" };
	const int typeIndex = std::clamp(uiManager_->blenderEnemyTypeIndex_, 0, static_cast<int>(std::size(enemyTypes)) - 1);
	simulationManager_->AddEnemySpawnToSceneJson(
		"resources/scene.json", uiManager_->blenderEnemyName_, enemyTypes[typeIndex], position, rotation);
#endif
}

void GamePlayScene::ReloadSceneJson() {
	bossSpawned_ = false;
	ClearAllEnemyReferences();
	if (lockOnManager_) {
		lockOnManager_->CancelMultiLock();
	}
	enemies_.clear();
	obstacles_.clear();
	enemySpawns_.clear();

	StageLoader::LoadSceneJson("resources/scene.json", enemies_, obstacles_, player_.get(), &enemySpawns_);
	if (IsTitleBackgroundMode()) {
		// タイトル背景は戦闘の見せ場用なので、VF1 以外や増援条件付きの敵を持ち込まない。
		// VF1 は最初から出しておき、タイトルを開いた瞬間からロックオン対象を作る。
		enemySpawns_.erase(
			std::remove_if(enemySpawns_.begin(), enemySpawns_.end(), [](const EnemySpawnData& spawn) {
				return spawn.enemyType != "VF1";
			}),
			enemySpawns_.end());
		for (EnemySpawnData& spawn : enemySpawns_) {
			spawn.isInitialSpawn = true;
			spawn.hasSpawned = false;
			spawn.reinforcementTriggerNames.clear();
			spawn.remainingReinforcementTriggers.clear();
		}
	}
	enemyRespawnTimers_.assign(enemySpawns_.size(), kNoEnemyRespawnTimer);


	enemyEventManager_.LoadEvents("resources/enemy_events.json");
	// Blender/StageLoaderで設定された初期スポーン設定(isInitialSpawn)をそのまま尊重する

	// 障害物のメッシュコライダー等を事前に構築・更新（敵の着地スナップ前に必須）
	for (auto& obstacle : obstacles_) {
		if (obstacle) {
			obstacle->Update();
		}
	}

	for (size_t i = 0; i < enemySpawns_.size(); ++i) {
		if (enemySpawns_[i].isInitialSpawn) {
			SpawnEnemyFromSpawnPoint(i);
		}
	}

	// 地上雑魚敵 5 体の直出し配置 (Blender未配置のハードコード敵は出さない)
	// SpawnDefaultGroundEnemies();

	try {
		lastJsonWriteTime_ = std::filesystem::last_write_time("resources/scene.json");
	} catch (...) {

	}
}

void GamePlayScene::ResetEditorPreview() {
	isEditorPreviewPlaying_ = false;
	isGameOver_ = false;
	gameOverTimer_ = 0;
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

	ReloadSceneJson();

	if (!isDebugCameraActive_ && player_) {
		Vector3* targetPos = nullptr;
		Vector3 enemyPos;
		if (lockedEnemy_) {
			enemyPos = lockedEnemy_->GetPosition();
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

void GamePlayScene::SpawnEnemyFromSpawnPoint(size_t spawnPointIndex) {
	if (spawnPointIndex >= enemySpawns_.size()) {
		return;
	}

	EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
	spawnData.hasSpawned = true;
	std::unique_ptr<Enemy> enemy;
	if (spawnData.isBoss) {
		enemy = std::make_unique<Boss>();
	} else if (spawnData.isJammer) {
		enemy = std::make_unique<JammerEnemy>();
	} else if (spawnData.isGround) {
		enemy = std::make_unique<GroundEnemy>();
	} else {
		enemy = std::make_unique<Enemy>();
	}

	enemy->Initialize(spawnData.position);
	enemy->SetRotation(spawnData.rotation);
	if (spawnData.flightPath.IsValid()) {
		enemy->SetFlightPath(spawnData.flightPath.points, spawnData.flightPath.loop, spawnData.flightPath.speed);
	}
	enemy->SetSpawnPointIndex(spawnPointIndex);

	if (GroundEnemy *ge = dynamic_cast<GroundEnemy *>(enemy.get())) {
		ge->SnapToGround(obstacles_);
	}

	enemies_.push_back(std::move(enemy));

	if (spawnPointIndex < enemyRespawnTimers_.size()) {
		enemyRespawnTimers_[spawnPointIndex] = kNoEnemyRespawnTimer;
	}
}

void GamePlayScene::SpawnDefaultGroundEnemies() {
	// Blender未配置のハードコード敵は生成しない
}

bool GamePlayScene::IsEnemySpawnPointActive(size_t spawnPointIndex) const {
	for (const auto &enemy : enemies_) {
		if (enemy && enemy->GetSpawnPointIndex() == spawnPointIndex && !enemy->IsDead()) {
			return true;
		}
	}
	return false;
}

void GamePlayScene::ScheduleEnemySpawn(size_t spawnPointIndex, int delayFrames) {
	if (spawnPointIndex >= enemySpawns_.size()) {
		return;
	}
	// すでに出現済みの敵は増援等で二重スポーンさせない
	if (enemySpawns_[spawnPointIndex].hasSpawned) {
		return;
	}
	if (enemyRespawnTimers_.size() < enemySpawns_.size()) {
		enemyRespawnTimers_.resize(enemySpawns_.size(), kNoEnemyRespawnTimer);
	}
	if (enemyRespawnTimers_[spawnPointIndex] != kNoEnemyRespawnTimer || IsEnemySpawnPointActive(spawnPointIndex)) {
		return;
	}

	enemyRespawnTimers_[spawnPointIndex] = delayFrames > 0 ? delayFrames : 1;
}

void GamePlayScene::TriggerEnemyReinforcements(const std::string &deadEnemyName) {
	if (deadEnemyName.empty()) {
		return;
	}

	const std::vector<EnemyEvent> events = enemyEventManager_.GetEventsForTrigger(deadEnemyName);
	for (const EnemyEvent &event : events) {
		for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
			if (enemySpawns_[spawnPointIndex].name == event.targetEnemyName) {
				ScheduleEnemySpawn(spawnPointIndex, event.delayFrames);
				break;
			}
		}
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemySpawns_.size(); ++spawnPointIndex) {
		EnemySpawnData &spawnData = enemySpawns_[spawnPointIndex];
		if (!spawnData.remainingReinforcementTriggers.empty()) {
			auto it = std::find(spawnData.remainingReinforcementTriggers.begin(), spawnData.remainingReinforcementTriggers.end(), deadEnemyName);
			if (it != spawnData.remainingReinforcementTriggers.end()) {
				spawnData.remainingReinforcementTriggers.erase(it);
				if (spawnData.remainingReinforcementTriggers.empty()) {
					ScheduleEnemySpawn(spawnPointIndex, spawnData.reinforcementDelayFrames);
				}
			}
		}
	}
}

void GamePlayScene::UpdateEnemyRespawns() {
	if (enemyRespawnTimers_.size() < enemySpawns_.size()) {
		enemyRespawnTimers_.resize(enemySpawns_.size(), kNoEnemyRespawnTimer);
	}

	for (size_t spawnPointIndex = 0; spawnPointIndex < enemyRespawnTimers_.size(); ++spawnPointIndex) {
		int &timer = enemyRespawnTimers_[spawnPointIndex];
		if (timer == kNoEnemyRespawnTimer) {
			continue;
		}
		if (timer > 0) {
			--timer;
		}
		if (timer == 0) {
			SpawnEnemyFromSpawnPoint(spawnPointIndex);
		}
	}
}

bool GamePlayScene::HasPendingEnemySpawns() const {
	for (int timer : enemyRespawnTimers_) {
		if (timer != kNoEnemyRespawnTimer) {
			return true;
		}
	}
	return false;
}





























MissileTuning GamePlayScene::MakeMissileTuning(MissileType type) const {
	MissileTuning tuning;
	if (type == MissileType::Normal) {
		tuning.speed = missileNormalSpeed;
		tuning.homingStrength = 0.0f;
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

void GamePlayScene::Update() {


	if (EditorReceiver::GetInstance()->Update(player_.get(), enemies_, obstacles_, enemySpawns_)) {
		// エディタからの差し替えでは敵リスト全体が再生成されるため、生ポインタの参照を残さない。
		ClearAllEnemyReferences();
		// Blenderで設定された初期スポーン設定(isInitialSpawn)をそのまま尊重する
	}


	// =========================================================

	// =========================================================
	static uint32_t jsonCheckFrameCounter = 0;
	// scene.json の監視は編集用シミュレーションだけで行う。
	// 通常プレイ中に OneDrive 上のファイルへ定期アクセスしないようにする。
	if (IsSimulationMode() && ImGuiManager::IsVisible() && ++jsonCheckFrameCounter % 30 == 0) {
		try {
			auto currentTime = std::filesystem::last_write_time("resources/scene.json");
			if (currentTime > lastJsonWriteTime_) {
				ReloadSceneJson();
				OutputDebugStringA("Hot Reloaded: scene.json\n");
			}
		} catch (...) {}
	}

	// タイトルメニューの入力を本編背景側へ渡さない。
	const bool canUseKeyboardInput = !IsTitleBackgroundMode() && !IsImGuiKeyboardCaptureActive();
	const bool canUseMouseInput = !IsTitleBackgroundMode() && !IsImGuiMouseCaptureActive();
	const bool canUsePlayerInput = canUseKeyboardInput && canUseMouseInput;
	if (!IsSimulationMode() && !IsTitleBackgroundMode() && !isGameOver_ && canUseKeyboardInput) {
		if (Input::GetInstance()->TriggerKey(DIK_ESCAPE)) {
			// Esc はいつでも素早くゲームへ戻れるショートカットにする。
			isPaused_ = !isPaused_;
			if (isPaused_) {
				isPauseTitleSelected_ = false;
			}
		} else if (isPaused_) {
			if (Input::GetInstance()->TriggerKey(DIK_UP) ||
				Input::GetInstance()->TriggerKey(DIK_DOWN) ||
				Input::GetInstance()->TriggerKey(DIK_W) ||
				Input::GetInstance()->TriggerKey(DIK_S)) {
				isPauseTitleSelected_ = !isPauseTitleSelected_;
			}
			if (Input::GetInstance()->TriggerKey(DIK_RETURN)) {
				if (isPauseTitleSelected_) {
					SceneManager::GetInstance()->ChangeScene("TITLE");
					return;
				}
				isPaused_ = false;
			}
		}
	}

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_0)) {
		OutputDebugStringA("HIt 0\n");
	}

	if (canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_F2)) {
		if (IsSimulationMode()) {
			PostQuitMessage(0);
		} else {
			LaunchSimulationExecutable();
		}
		return;
	}

	if (canUseKeyboardInput && IsSimulationMode() && Input::GetInstance()->TriggerKey(DIK_F3)) {
		SetDebugCameraActive(!isDebugCameraActive_);
	}


	if (!isPaused_ && canUseKeyboardInput && Input::GetInstance()->TriggerKey(DIK_R)) {
		SceneManager::GetInstance()->ChangeScene(IsSimulationMode() ? "SIMULATION" : "GAMEPLAY");
		return;
	}

	// ==========================================

	// ==========================================
	if (!IsSimulationMode() && !IsTitleBackgroundMode() && !isGameOver_ && player_ && player_->IsDead()) {
		isGameOver_ = true;
		gameOverTimer_ = 0;

		std::vector<Vector3> playerHitPos = { player_->GetPosition() };
		if (explosionManager_) {
			explosionManager_->CreateDestructionEffects(playerHitPos);
		}

		if (pVoice2) {
			pVoice2->Stop();
		}
	}

	bool shouldUpdateGame = !isPaused_;

	if (isGameOver_) {
		gameOverTimer_++;


		if (PostEffect::GetInstance()) {
			float effectProgress = static_cast<float>(gameOverTimer_) / 120.0f;
			if (effectProgress > 1.0f) {
				effectProgress = 1.0f;
			}
			float vignetteRadius = 0.62f - 0.22f * effectProgress;
			float blurIntensity = 1.5f + 3.0f * effectProgress;
			PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.38f, blurIntensity);
		}


		shouldUpdateGame = (gameOverTimer_ % 5 == 0);


		if (gameOverTimer_ >= 120) {
			SceneManager::GetInstance()->ChangeScene("GAMEOVER");
		}
	} else {

		if (PostEffect::GetInstance()) {
			bool isBoosting = false;
			if (player_ && player_->GetCurrentMode() == PlayerMode::Fighter) {
				float maxSpeed = player_->GetModeParams(PlayerMode::Fighter).maxMoveSpeed;
				float speed = MyMath::Length(player_->GetVelocity());
				if (speed > maxSpeed * 1.5f) {
					isBoosting = true;
					float effectProgress = std::clamp((speed - maxSpeed * 1.5f) / (maxSpeed * 3.0f - maxSpeed * 1.5f), 0.0f, 1.0f);
					float vignetteRadius = 0.5f - 0.1f * effectProgress;
					// 加速中は爆発・トレイルと重なりやすいため、全画面を9回読む
					// ぼかしではなく軽量なビネットだけを使う。
					PostEffect::GetInstance()->SetVignette(vignetteRadius, 0.4f);
				}
			}
			
			if (player_) {
				if (previousPlayerHP_ == -1) {
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() < previousPlayerHP_) {
					damageEffectTimer_ = 30;
					previousPlayerHP_ = player_->GetHP();
				} else if (player_->GetHP() > previousPlayerHP_) {
					previousPlayerHP_ = player_->GetHP();
				}
			}

			if (damageEffectTimer_ > 0) {
				damageEffectTimer_--;
				PostEffect::GetInstance()->SetEffectType(13); // Fold Wave
			} else if (!isBoosting) {
				PostEffect::GetInstance()->SetEffectType(0); // 0: Normal
			}
		}
	}

	// 上部ツールバーの「再生／編集に戻す」は、通常ゲーム画面と
	// シミュレーション画面のどちらでも共通の停止スイッチとして扱う。
	// 以前は通常ゲーム画面ではこのフラグを無視していたため、
	// 「編集モード（停止中）」でもゲームだけ進行してしまっていた。
	shouldUpdateGame = shouldUpdateGame && isEditorPreviewPlaying_;
	const bool isSimulation = IsSimulationMode();
	const bool isFullFlowPreview = !isSimulation || uiManager_->simulationPlaybackMode_ == 1;
	const bool isSelectedOnlyPreview = isSimulation && uiManager_->simulationPlaybackMode_ == 0;
	// 移動キーは設定パネル上にマウスがあっても受け付ける。
	// マウス捕捉まで必須にすると、UI を表示しているだけで Player::Update が
	// 呼ばれず、W/S などのキーボード移動まで停止してしまう。
	const bool updateSelectedPlayer = shouldUpdateGame && canUseKeyboardInput && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 0);
	const bool updateSelectedMissiles = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 1);
	const bool updateSelectedEnemies = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 2);
	const bool updateSelectedParticles = shouldUpdateGame && (isFullFlowPreview || uiManager_->currentSimulationTarget_ == 3);
	const bool allowWeaponInput = shouldUpdateGame && canUsePlayerInput && (!isSimulation || isFullFlowPreview);
	const bool allowLockOnBehavior = shouldUpdateGame && !isGameOver_ &&
		(isFullFlowPreview || uiManager_->currentSimulationTarget_ == 1 || uiManager_->currentSimulationTarget_ == 2);
	const bool updateDebugWireframes = !isSimulation || isFullFlowPreview || updateSelectedPlayer || updateSelectedMissiles || updateSelectedEnemies || updateSelectedParticles;
	const bool updateAnimationPreview = shouldUpdateGame && (!isSimulation || isFullFlowPreview);

	const bool isAnimationEditor = isSimulation && uiManager_->currentSimulationTarget_ == 5;
	static bool wasAnimationEditor = false;
	if (isAnimationEditor && !wasAnimationEditor) {
		SetDebugCameraActive(true);
		if (player_ && debugFlyCamera_) {
			Vector3 pPos = player_->GetPosition();

			debugFlyCamera_->SetTranslate({ pPos.x, pPos.y + 2.0f, pPos.z - 12.0f });
			debugFlyCamera_->SetQuaternion({ 0.0f, 0.0f, 0.0f, 1.0f });
		}
	} else if (!isAnimationEditor && wasAnimationEditor) {
		SetDebugCameraActive(false);
	}
	wasAnimationEditor = isAnimationEditor;

	if (shouldUpdateGame && canUsePlayerInput && !isSpecialAttackActive_) {
		UpdateAutoReload();
	}


	if (!isGameOver_ && shouldUpdateGame && canUsePlayerInput &&
		!isSpecialAttackActive_ && spGauge_ >= kSpecialAttackCost &&
		Input::GetInstance()->TriggerAction(PlayerAction::SpecialAttack)) {
		spGauge_ -= kSpecialAttackCost;
		isSpecialAttackActive_ = true;
		specialAttackFrame_ = 0;
		if (player_) {
			player_->SetSpecialAttackActive(true);
		}
		// 必殺技発動時にターゲット未選択の場合、最も近い生存敵を自動ロックオンする
		if (!lockedEnemy_ || lockedEnemy_->IsDead()) {
			lockedEnemy_ = nullptr;
			float minDistanceSq = 100000000.0f;
			Enemy* nearestEnemy = nullptr;
			const Vector3 playerPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
			for (const auto& enemy : enemies_) {
				if (enemy && !enemy->IsDead()) {
					const float dSq = LengthSqVector3(SubtractVector3(enemy->GetPosition(), playerPos));
					if (dSq < minDistanceSq) {
						minDistanceSq = dSq;
						nearestEnemy = enemy.get();
					}
				}
			}
			if (nearestEnemy) {
				lockedEnemy_ = nearestEnemy;
			}
		}
	}


	if (!isGameOver_ && shouldUpdateGame && canUsePlayerInput &&
		!isSongActive_ && songGauge_ >= 100.0f &&
		Input::GetInstance()->TriggerAction(PlayerAction::Song)) {
		songGauge_ = 0.0f;
		isSongActive_ = true;
		songFrame_ = 0;
		if (player_) {
			player_->SetSongActive(true);
		}
		if (pVoice2) pVoice2->SetVolume(0.0f);
		if (pSongVoice) pSongVoice->SetVolume(1.0f);
	}

	if (isSongActive_) {
		++songFrame_;
		if (songFrame_ >= 900) { // 15 seconds
			isSongActive_ = false;
			songFrame_ = 0;
			if (player_) {
				player_->SetSongActive(false);
			}
			if (pVoice2) pVoice2->SetVolume(1.0f);
			if (pSongVoice) pSongVoice->SetVolume(0.0f);
		}
	}

	if (isSpecialAttackActive_) {
		// 必殺技中にターゲット敵が死亡した場合、最寄りの次の生存敵へ自動的にターゲットを切り替えて追従を維持する
		if (!lockedEnemy_ || lockedEnemy_->IsDead()) {
			lockedEnemy_ = nullptr;
			float minDistanceSq = 100000000.0f;
			Enemy* nearestEnemy = nullptr;
			const Vector3 playerPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
			for (const auto& enemy : enemies_) {
				if (enemy && !enemy->IsDead()) {
					const float dSq = LengthSqVector3(SubtractVector3(enemy->GetPosition(), playerPos));
					if (dSq < minDistanceSq) {
						minDistanceSq = dSq;
						nearestEnemy = enemy.get();
					}
				}
			}
			if (nearestEnemy) {
				lockedEnemy_ = nearestEnemy;
			}
		}

		if (specialAttackFrame_ % kSpecialAttackFireIntervalFrames == 0 && missilePresetManager_) {

			// 必殺技は SP 消費で成立する攻撃なので、通常弾・誘導弾の残弾には依存させない。
			missilePresetManager_->FirePlayerMissile(MissileType::Normal, nullptr, -0.3f, false);

			missilePresetManager_->FirePlayerMissile(MissileType::MissileWithTrail, nullptr, 0.3f, false);
		}
		++specialAttackFrame_;
		if (specialAttackFrame_ >= kSpecialAttackDurationFrames) {
			isSpecialAttackActive_ = false;
			specialAttackFrame_ = 0;
			if (player_) {
				player_->SetSpecialAttackActive(false);
			}
		}
	} else {
		spGauge_ = std::clamp(spGauge_ + kSpGaugeRecoveryPerFrame, 0.0f, 100.0f);
	}

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


		bool isAnimationEditor = IsSimulationMode() && uiManager_ && uiManager_->currentSimulationTarget_ == 5;
		if ((showBones || isAnimationEditor) && player_) {
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



	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	lockOnManager_->UpdateLockOn(activeCamera, allowLockOnBehavior);


	if (player_) {
		if (IsTitleBackgroundMode() && titleLaunchActive_) {
			// START GAME 後もタイトルで見えていた地形を維持し、機体だけを一度ロールさせて出撃させる。
			constexpr int kTitleLaunchDurationFrames = 84;
			const float progress = std::clamp(
				static_cast<float>(titleLaunchFrame_) / static_cast<float>(kTitleLaunchDurationFrames), 0.0f, 1.0f);
			// タイトル背景の巡航方向を変えずに加速する。カメラへ向かう補間や
			// ベジェ旋回を入れないため、画面下へ沈む不自然な軌道にならない。
			const float distance = progress * 8.0f + progress * progress * 30.0f;
			const Vector3 position = AddVector3(
				titleLaunchStartPosition_, ScaleVector3(titleLaunchForward_, distance));
			const float yaw = std::atan2(titleLaunchForward_.x, titleLaunchForward_.z);
			const float roll = progress * 6.28318531f;
			player_->SetVelocity({
				titleLaunchForward_.x * 0.78f,
				titleLaunchForward_.y * 0.78f,
				titleLaunchForward_.z * 0.78f,
			});
			player_->UpdatePresentation(position, { -0.05f, yaw, roll }, 2.2f, true);

			// 地形の近くを抜けるタイミングだけ、薄い霧を散らして速度感を補う。
			if (environmentRenderer_ && titleLaunchFrame_ % 12 == 0) {
				environmentRenderer_->GetParticleManager()->Emit(
					"smoke", position, 2, { 0.72f, 0.80f, 0.88f, 0.11f },
					0.035f, 0.015f, 3.8f, 1.4f, 1.6f, 2.8f, 7.0f);
			}
			++titleLaunchFrame_;
		} else if (IsTitleBackgroundMode()) {
			// StageBounds 内を「直線 + 壁際のUターン」で周回する。
			// 壁の手前で大きくバンクして旋回するため、タイトルを放置しても
			// フィールド外へ飛び出さず、直線飛行の見せ場も作れる。
			Vector3 stageCenter = { 0.0f, 0.0f, 0.0f };
			Vector3 stageHalfExtents = { 50.0f, 30.0f, 50.0f };
			if (const auto stageBoundsIt = std::find_if(obstacles_.begin(), obstacles_.end(),
				[](const std::unique_ptr<Obstacle>& obstacle) {
					return obstacle && obstacle->IsStageBounds();
				}); stageBoundsIt != obstacles_.end()) {
				stageCenter = (*stageBoundsIt)->GetPosition();
				stageHalfExtents = (*stageBoundsIt)->GetWorldHalfExtents();
			}

			const Vector3 playerHalfExtents = player_->GetWorldHalfExtents();
			const float time = static_cast<float>(titleBackgroundFrame_++) * (1.0f / 60.0f);
			// ふだんは安全な余白を保ち、約3周に1回だけ壁をかすめる近距離パスにする。
			// 値を連続的に変えることで、コース切替時に機体がワープしない。
			const float closePassBlend = std::pow((std::max)(0.0f, std::sin(time * 0.14f)), 6.0f);
			const float stageMargin = 5.0f - 3.75f * closePassBlend;
			const float usableX = (std::max)(2.0f, stageHalfExtents.x - playerHalfExtents.x - stageMargin);
			const float usableZ = (std::max)(2.0f, stageHalfExtents.z - playerHalfExtents.z - stageMargin);
			const float orbitRadiusX = (std::min)(38.0f, usableX * (0.88f + 0.10f * closePassBlend));
			const float turnRadius = (std::min)(18.0f, usableZ * (0.88f + 0.10f * closePassBlend));
			const float verticalRange = (std::min)(2.0f, (std::max)(0.0f, stageHalfExtents.y - playerHalfExtents.y - stageMargin));
			const float baseHeight = stageCenter.y + (std::min)(3.5f, verticalRange);
			const float verticalBob = verticalRange > 0.1f ? std::sin(time * 0.85f) * verticalRange * 0.28f : 0.0f;
			const float straightHalfLength = (std::max)(0.0f, orbitRadiusX - turnRadius);
			const float straightLength = straightHalfLength * 2.0f;
			const float turnLength = 3.14159265f * turnRadius;
			const float lapLength = (std::max)(1.0f, straightLength * 2.0f + turnLength * 2.0f);
			constexpr float kCruiseSpeedPerSecond = 10.0f;
			const float lapDistance = std::fmod(time * kCruiseSpeedPerSecond, lapLength);
			Vector3 position = { stageCenter.x - straightHalfLength, baseHeight + verticalBob, stageCenter.z - turnRadius };
			Vector3 horizontalVelocity = { kCruiseSpeedPerSecond, 0.0f, 0.0f };
			float roll = 0.0f;
			if (lapDistance < straightLength) {
				position.x += lapDistance;
			} else if (lapDistance < straightLength + turnLength) {
				const float turnProgress = (lapDistance - straightLength) / (std::max)(0.001f, turnLength);
				const float angle = -1.57079633f + turnProgress * 3.14159265f;
				position.x = stageCenter.x + straightHalfLength + turnRadius * std::cos(angle);
				position.z = stageCenter.z + turnRadius * std::sin(angle);
				horizontalVelocity = { -std::sin(angle) * kCruiseSpeedPerSecond, 0.0f, std::cos(angle) * kCruiseSpeedPerSecond };
				roll = -std::sin(turnProgress * 3.14159265f) * 0.56f;
			} else if (lapDistance < straightLength * 2.0f + turnLength) {
				position.x = stageCenter.x + straightHalfLength - (lapDistance - straightLength - turnLength);
				position.z = stageCenter.z + turnRadius;
				horizontalVelocity = { -kCruiseSpeedPerSecond, 0.0f, 0.0f };
			} else {
				const float turnProgress = (lapDistance - straightLength * 2.0f - turnLength) / (std::max)(0.001f, turnLength);
				const float angle = 1.57079633f + turnProgress * 3.14159265f;
				position.x = stageCenter.x - straightHalfLength + turnRadius * std::cos(angle);
				position.z = stageCenter.z + turnRadius * std::sin(angle);
				horizontalVelocity = { -std::sin(angle) * kCruiseSpeedPerSecond, 0.0f, std::cos(angle) * kCruiseSpeedPerSecond };
				roll = std::sin(turnProgress * 3.14159265f) * 0.56f;
			}
			const Vector3 velocity = {
				horizontalVelocity.x / 60.0f,
				verticalRange > 0.1f ? std::cos(time * 0.85f) * verticalRange * 0.28f * 0.85f / 60.0f : 0.0f,
				horizontalVelocity.z / 60.0f,
			};
			const float horizontalSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
			const float yaw = std::atan2(velocity.x, velocity.z);
			const float pitch = horizontalSpeed > 0.0001f ? -std::atan2(velocity.y, horizontalSpeed) : 0.0f;
			player_->SetVelocity(velocity);
			player_->UpdatePresentation(position, { pitch, yaw, roll }, (std::max)(0.9f, horizontalSpeed * 7.0f), true);
			// タイトル地形にうっすら漂う霧。低頻度で少量だけ発生させ、視界を遮らない。
			if (environmentRenderer_ && titleBackgroundFrame_ % 18 == 0) {
				environmentRenderer_->GetParticleManager()->Emit(
					"smoke", position, 1, { 0.70f, 0.78f, 0.86f, 0.07f },
					0.018f, 0.008f, 4.8f, 1.6f, 2.5f, 4.0f, 9.0f);
			}
		} else if (updateSelectedPlayer) {
			if (introBoostFrames_ > 0) {
				const float progress = 1.0f - static_cast<float>(introBoostFrames_) /
					static_cast<float>((std::max)(1, introBoostInitialFrames_));
				Vector3 position = player_->GetPosition();
				position.x += introBoostVelocity_.x;
				position.y += introBoostVelocity_.y + std::sin(progress * 3.14159265f) * 0.02f;
				position.z += introBoostVelocity_.z;
				player_->SetVelocity(introBoostVelocity_);
				player_->UpdatePresentation(position, { -0.05f, introBoostYaw_, progress * 6.28318531f }, 2.2f, true);
				if (environmentRenderer_ && introBoostFrames_ % 8 == 0) {
					environmentRenderer_->GetParticleManager()->Emit(
						"smoke", position, 2, { 0.72f, 0.80f, 0.88f, 0.11f },
						0.035f, 0.015f, 3.8f, 1.4f, 1.6f, 2.8f, 7.0f);
				}
				--introBoostFrames_;
			} else {
				Vector3 lockOnTargetPosition;
				const Vector3 *lockOnTarget = nullptr;
				if (lockedEnemy_) {
					lockOnTargetPosition = lockedEnemy_->GetPosition();
					lockOnTarget = &lockOnTargetPosition;
				}
				player_->Update(obstacles_, lockOnTarget);
			}
		} else {
			// UI がキー入力を捕捉している間は、モデル更新だけを行い入力は渡さない。
			// 停止中もフリーカメラ用にWVPは更新するが、機体のアニメーションや補間は進めない。
			player_->UpdateModel(canUseKeyboardInput, shouldUpdateGame);
		}

		// ゲーム／シミュレーション共通の編集停止中はプレイヤーの状態を動かさず、
		// スラスターだけを専用プレビューとして更新する。
		if (!isEditorPreviewPlaying_ && !IsTitleBackgroundMode() && player_->GetBoosterEffect()) {
			player_->GetBoosterEffect()->UpdateEditorPreview(player_->GetPosition(), player_->GetQuaternion(),
				static_cast<int>(player_->GetCurrentMode()));
		}

	}

	// ==========================================

	// ==========================================

	Vector3 playerPos = player_ ? player_->GetOBB().center : Vector3{ 0.0f, 0.0f, 0.0f };

	if (updateSelectedEnemies) {

		std::vector<Vector3> enemyBulletHits;
		if (enemyBulletManager_) {
			// タイトル背景でも敵弾は必ず更新する。プレイヤーを nullptr にして、
			// 動きは見せつつタイトル画面でダメージを受けないようにする。
			enemyBulletManager_->Update(IsTitleBackgroundMode() ? nullptr : player_.get(), enemyBulletHits, obstacles_);
		}


		if (explosionManager_ && !enemyBulletHits.empty()) {
			explosionManager_->CreateHitEffects(enemyBulletHits);
		}

		// 敵同士が同じ場所に固まって1点に重なるのを防ぐ押し出し（群集離隔・分離）処理
		for (auto it1 = enemies_.begin(); it1 != enemies_.end(); ++it1) {
			if (!(*it1) || (*it1)->IsDead()) continue;
			Vector3 pos1 = (*it1)->GetPosition();
			float radius1 = (*it1)->GetCollisionRadius();
			if (radius1 <= 0.1f) radius1 = 1.2f;

			for (auto it2 = std::next(it1); it2 != enemies_.end(); ++it2) {
				if (!(*it2) || (*it2)->IsDead()) continue;
				// ボス本体は召喚直後の雑魚敵に押し動かされないようにする。
				if ((*it1)->IsBoss() || (*it2)->IsBoss()) continue;
				Vector3 pos2 = (*it2)->GetPosition();
				float radius2 = (*it2)->GetCollisionRadius();
				if (radius2 <= 0.1f) radius2 = 1.2f;

				Vector3 diff = SubtractVector3(pos1, pos2);
				diff.y = 0.0f; // 水平方向のみで押し広げる
				float distSq = LengthSqVector3(diff);
				float minDist = radius1 + radius2 + 3.0f; // 重なり防止距離

				if (distSq < minDist * minDist) {
					float dist = std::sqrt(distSq);
					Vector3 dir = { 1.0f, 0.0f, 0.0f };
					if (dist > 0.001f) {
						dir = { diff.x / dist, 0.0f, diff.z / dist };
					}
					float overlap = (minDist - dist) * 0.5f;

					Vector3 newPos1 = { pos1.x + dir.x * overlap, pos1.y, pos1.z + dir.z * overlap };
					Vector3 newPos2 = { pos2.x - dir.x * overlap, pos2.y, pos2.z - dir.z * overlap };

					(*it1)->SetPosition(newPos1);
					(*it2)->SetPosition(newPos2);
				}
			}
		}

		for (auto it = enemies_.begin(); it != enemies_.end(); ) {
			// 近接攻撃の当たり判定
			if (player_ && player_->IsMeleeAttacking()) {
				OBB meleeHitbox = player_->GetMeleeHitbox();
				Sphere enemySphere;
				enemySphere.center = (*it)->GetPosition();
				enemySphere.radius = (*it)->GetCollisionRadius();
				if (MyMath::IsCollision(enemySphere, meleeHitbox)) {
					(*it)->TakeDamage(player_->GetMeleeDamage());
				}
			}

			// 地上敵の近接攻撃当たり判定
			GroundEnemy* groundEnemy = dynamic_cast<GroundEnemy*>(it->get());
			if (!IsTitleBackgroundMode() && groundEnemy && groundEnemy->IsMeleeActive() && player_ && !player_->IsDead()) {
				OBB meleeOBB = groundEnemy->GetMeleeBoxOBB();
				OBB playerOBB = player_->GetOBB();
				if (MyMath::IsCollision(meleeOBB, playerOBB)) {
					player_->TakeDamage(1);
				}
			}

			(*it)->Update(playerPos, enemyBulletManager_.get(), obstacles_);
			if (Boss *boss = dynamic_cast<Boss *>(it->get())) {
				const int requestedCount = boss->ConsumeSummonRequests();
				const int activeMinionCount = static_cast<int>(std::count_if(
					enemies_.begin(), enemies_.end(),
					[](const std::unique_ptr<Enemy>& enemy) {
						return enemy && !enemy->IsDead() && enemy->IsBossMinion();
					}));
				const int summonCount = (std::min)(requestedCount, (std::max)(0, kBossMaxActiveMinions - activeMinionCount));
				for (int i = 0; i < summonCount; ++i) {
					auto escort = std::make_unique<Enemy>();
					// 召喚直後はボス本体の中心から出現し、その後に通常の分離処理で展開する。
					escort->Initialize(boss->GetPosition());
					escort->SetIsBossMinion(true);
					escort->StartChasingPlayer();
					enemies_.push_back(std::move(escort));
				}
			}
			if ((*it)->IsDead()) {
				const bool defeatedBoss = (*it)->IsBoss();
				const Vector3 defeatedPosition = (*it)->GetPosition();
				const size_t spawnPointIndex = (*it)->GetSpawnPointIndex();
				if (IsTitleBackgroundMode()) {
					// タイトル背景でも撃破された敵は残さない。次の VF1 をロックオン対象に切り替える。
					ClearEnemyReferences(it->get());
					it = enemies_.erase(it);
					continue;
				}
				ClearEnemyReferences(it->get());
				
				if (spawnPointIndex < enemySpawns_.size()) {
					const std::string& deadName = enemySpawns_[spawnPointIndex].name;
					TriggerEnemyReinforcements(deadName);
				}

				if (spawnPointIndex < enemySpawns_.size() && enemySpawns_[spawnPointIndex].isInitialSpawn) {
// 					ScheduleEnemySpawn(spawnPointIndex, kEnemyRespawnDelayFrames);
				}
				songGauge_ = (std::min)(songGauge_ + 20.0f, 100.0f);
				it = enemies_.erase(it);


				if (defeatedBoss && !IsSimulationMode() && !IsTitleBackgroundMode() && !isGameOver_) {
					SceneManager::GetInstance()->ChangeScene("CLEAR");
					return;
				}
			} else {
				++it;
			}
		}

		if (IsTitleBackgroundMode() && player_) {
			if (!lockedEnemy_ || lockedEnemy_->IsDead()) {
				lockedEnemy_ = nullptr;
				for (const auto& enemy : enemies_) {
					if (enemy && !enemy->IsDead()) {
						lockedEnemy_ = enemy.get();
						break;
					}
				}
			}

			if (lockedEnemy_) {
				// ロックオン対象は常に機体前方を横切らせ、タイトル上でも視認・照準できる距離に保つ。
				const float time = static_cast<float>(titleBackgroundFrame_) * (1.0f / 60.0f);
				const Vector3 forward = NormalizeOrVector3(player_->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
				const Vector3 right = NormalizeOrVector3(MyMath::Cross({ 0.0f, 1.0f, 0.0f }, forward), { 1.0f, 0.0f, 0.0f });
				Vector3 targetPosition = player_->GetPosition();
				const float forwardDistance = 25.0f + std::sin(time * 0.7f) * 4.0f;
				const float lateralDistance = std::sin(time * 1.15f) * 7.0f;
				targetPosition.x += forward.x * forwardDistance + right.x * lateralDistance;
				// ロゴは画面上部にあるため、敵は機体より少し低い高度を横切らせる。
				// 後段のスクリーン座標補正と合わせ、タイトルと重なりにくくする。
				targetPosition.y -= 1.2f + std::sin(time * 1.6f) * 0.7f;
				targetPosition.z += forward.z * forwardDistance + right.z * lateralDistance;

				// 現在の追従カメラで画面上部（タイトルロゴの領域）に入る場合は、
				// カメラの下方向へずらして中央の戦闘スペースに戻す。
				player_->UpdateCamera(camera.get(), nullptr, false);
				camera->Update();
				const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
				const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
				const Vector3 screenPosition = MyMath::WorldToScreen(targetPosition, camera->GetViewProjectionMatrix(), screenWidth, screenHeight);
				if (screenPosition.z > 0.0f && screenPosition.z < 1.0f) {
					constexpr float kSafeTop = 0.35f;
					constexpr float kSafeBottom = 0.63f;
					const float desiredScreenY = std::clamp(screenPosition.y, screenHeight * kSafeTop, screenHeight * kSafeBottom);
					const float screenDelta = desiredScreenY - screenPosition.y;
					if (std::abs(screenDelta) > 1.0f) {
						const Vector3 cameraForward = NormalizeOrVector3(
							SubtractVector3(player_->GetOBB().center, camera->GetTranslate()),
							forward);
						const Vector3 cameraRight = NormalizeOrVector3(MyMath::Cross({ 0.0f, 1.0f, 0.0f }, cameraForward), right);
						const Vector3 cameraUp = NormalizeOrVector3(MyMath::Cross(cameraForward, cameraRight), { 0.0f, 1.0f, 0.0f });
						const float worldOffset = std::clamp(screenDelta / screenHeight * 18.0f, -4.0f, 4.0f);
						targetPosition.x -= cameraUp.x * worldOffset;
						targetPosition.y -= cameraUp.y * worldOffset;
						targetPosition.z -= cameraUp.z * worldOffset;
					}
				}

				if (const auto stageBoundsIt = std::find_if(obstacles_.begin(), obstacles_.end(),
					[](const std::unique_ptr<Obstacle>& obstacle) {
						return obstacle && obstacle->IsStageBounds();
					}); stageBoundsIt != obstacles_.end()) {
					const Vector3 center = (*stageBoundsIt)->GetPosition();
					const Vector3 halfExtents = (*stageBoundsIt)->GetWorldHalfExtents();
					const Vector3 enemyHalfExtents = lockedEnemy_->GetWorldHalfExtents();
					auto clampInsideStage = [](float value, float centerValue, float stageHalfExtent, float objectHalfExtent) {
						constexpr float kMargin = 1.5f;
						const float minimum = centerValue - stageHalfExtent + objectHalfExtent + kMargin;
						const float maximum = centerValue + stageHalfExtent - objectHalfExtent - kMargin;
						return minimum <= maximum ? std::clamp(value, minimum, maximum) : centerValue;
					};
					targetPosition.x = clampInsideStage(targetPosition.x, center.x, halfExtents.x, enemyHalfExtents.x);
					targetPosition.y = clampInsideStage(targetPosition.y, center.y, halfExtents.y, enemyHalfExtents.y);
					targetPosition.z = clampInsideStage(targetPosition.z, center.z, halfExtents.z, enemyHalfExtents.z);
				}

				lockedEnemy_->SetPosition(targetPosition);
				lockedEnemy_->SetRotation({ 0.0f, std::atan2(-forward.x, -forward.z), 0.0f });
				lockedEnemy_->UpdateModel();
				if (missilePresetManager_ && titleBackgroundFrame_ % 90 == 0) {
					missilePresetManager_->FirePlayerMissile(MissileType::Normal, lockedEnemy_, -0.35f, false);
				}
				if (missilePresetManager_ && titleBackgroundFrame_ % 240 == 0) {
					missilePresetManager_->FirePlayerMissile(MissileType::MissileWithTrail, lockedEnemy_, 0.35f, false);
				}
			}
		}
		UpdateEnemyRespawns();

		bool hasAnyEnemySpawned = false;
		for (const auto& spawn : enemySpawns_) {
			if (!spawn.isBoss && spawn.hasSpawned) {
				hasAnyEnemySpawned = true;
				break;
			}
		}

		if (!IsSimulationMode() && !IsTitleBackgroundMode() && !isGameOver_ && hasAnyEnemySpawned && enemies_.empty() && !HasPendingEnemySpawns()) {
			if (!bossSpawned_) {
				OutputDebugStringA("[GamePlayScene] All enemies defeated! Spawning Boss...\n");
				auto boss = std::make_unique<Boss>();
			boss->Initialize(playerPos);

			// StageBounds の外へ出ないよう、ステージ内のプレイヤー前方寄りに配置する。
			// ボス自身の半径と余白も確保するため、モデル全体が境界内に収まる。
			if (const auto stageBoundsIt = std::find_if(obstacles_.begin(), obstacles_.end(),
				[](const std::unique_ptr<Obstacle>& obstacle) {
					return obstacle && obstacle->IsStageBounds();
				}); stageBoundsIt != obstacles_.end()) {
				const Obstacle& stageBounds = **stageBoundsIt;
				const Vector3 center = stageBounds.GetPosition();
				const Vector3 halfExtents = stageBounds.GetWorldHalfExtents();
				const Vector3 bossHalfExtents = boss->GetWorldHalfExtents();
				const Vector3 playerForward = player_->GetForwardVector();
				constexpr float kBoundaryMargin = 2.0f;
				auto clampInsideStage = [](float desired, float centerValue, float stageHalfExtent, float bossHalfExtent) {
					const float minValue = centerValue - stageHalfExtent + bossHalfExtent + kBoundaryMargin;
					const float maxValue = centerValue + stageHalfExtent - bossHalfExtent - kBoundaryMargin;
					return minValue <= maxValue ? std::clamp(desired, minValue, maxValue) : centerValue;
				};

				boss->SetPosition({
					clampInsideStage(playerPos.x + playerForward.x * 24.0f, center.x, halfExtents.x, bossHalfExtents.x),
					clampInsideStage(playerPos.y + 14.0f, center.y, halfExtents.y, bossHalfExtents.y),
					clampInsideStage(playerPos.z + playerForward.z * 24.0f, center.z, halfExtents.z, bossHalfExtents.z),
				});
				boss->UpdateModel();
			}
				enemies_.push_back(std::move(boss));
				bossSpawned_ = true;
			} else {
				OutputDebugStringA("[GamePlayScene] Boss defeated! Changing scene to CLEAR.\n");
				SceneManager::GetInstance()->ChangeScene("CLEAR");
				return;
			}
		}


		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	} else {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
	}


	if (isDebugCameraActive_) {
		debugFlyCamera_->SetCanUseKeyboard(canUseKeyboardInput);
		debugFlyCamera_->Update();

		// Object3d は Update 時点のカメラ行列を WVP に保持する。
		// 停止中のプレビューでも、フリーカメラを動かした後に地形を含む
		// 障害物の WVP を更新しないと、旧カメラの位置でカリングされてしまう。
		for (auto& obstacle : obstacles_) {
			obstacle->Update();
		}
		TryPlaceEnemyAtGameViewMouse();
		
		if (isAnimationEditor && simulationManager_) {
			simulationManager_->UpdateShortcuts();
			ImGuiIO& io = ImGui::GetIO();
			Vector2 localMousePos;
			
			static bool s_clickedOnBone = false;
			static bool s_isDraggingBone = false;
			static ImVec2 s_mouseDownPos = {0, 0};
			
			if (FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, localMousePos)) {
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					float gw, gh;
					FlyCamera::GetGameViewSize(gw, gh);
					Ray ray = MyMath::ScreenToRay(localMousePos, gw, gh, MyMath::Inverse(debugFlyCamera_->GetViewProjectionMatrix()));
					
					float closestDist = 999999.0f;
					std::string closestBone = "";
					
					if (this->player_) {
						const Skeleton& playerSkeleton = this->player_->GetSkeleton();
						for (const auto& joint : playerSkeleton.joints) {
							Vector3 pos = {
								joint.skeletonSpaceMatrix.m[3][0],
								joint.skeletonSpaceMatrix.m[3][1],
								joint.skeletonSpaceMatrix.m[3][2]
							};
						
							Vector3 currentScale = {1.0f, 1.0f, 1.0f};
							if (this->player_ && this->player_->GetObject3d()) {
								currentScale = this->player_->GetObject3d()->GetScale();
								Matrix4x4 worldMat = MyMath::MakeAffineMatrix(currentScale, this->player_->GetQuaternion(), this->player_->GetPosition());
								pos = MyMath::Transform(pos, worldMat);
							}

							Sphere s;
							s.center = pos;
							s.radius = 0.5f * currentScale.x;
						
						float dist;
						if (MyMath::IntersectRaySphere(ray, s, &dist)) {
							if (dist < closestDist) {
								closestDist = dist;
								closestBone = joint.name;
							}
						}
					}
					}
					
					s_clickedOnBone = !closestBone.empty();
					s_mouseDownPos = io.MousePos;

					if (s_clickedOnBone) {
						if (io.KeyShift) {
							if (simulationManager_->IsBoneSelected(closestBone)) {
								simulationManager_->RemoveSelectedBoneName(closestBone);
							} else {
								simulationManager_->AddSelectedBoneName(closestBone);
							}
						} else {

							if (!simulationManager_->IsBoneSelected(closestBone)) {
								simulationManager_->ClearSelectedBones();
								simulationManager_->AddSelectedBoneName(closestBone);
							}
						}
						isBoxSelecting_ = false;
						s_isDraggingBone = true;
					} else {

						if (io.KeyShift || simulationManager_->GetSelectedBoneNames().empty()) {

							isBoxSelecting_ = true;
							s_isDraggingBone = false;
							boxSelectStartPos_ = localMousePos;
							boxSelectEndPos_ = localMousePos;
						} else {

							isBoxSelecting_ = false;
							s_isDraggingBone = true;
						}
					}
				}
				
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
					const auto& selBones = simulationManager_->GetSelectedBoneNames();
					float dx = io.MouseDelta.x;
					float dy = io.MouseDelta.y;
					
					if (isBoxSelecting_) {
						Vector2 localMousePos;
						if (FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, localMousePos)) {
							boxSelectEndPos_ = localMousePos;
						}
					} else if (s_isDraggingBone && !selBones.empty()) {
						if (dx != 0.0f || dy != 0.0f) {
							bool doTranslate = io.KeyCtrl;
							for (const auto& boneName : selBones) {
								if (doTranslate) {
									simulationManager_->AddBoneTranslationFromDrag(boneName, dx, dy);
								} else {
									simulationManager_->AddBoneRotationFromDrag(boneName, dx, dy);
								}
							}
						}
					}
				}

				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					if (isBoxSelecting_) {
						isBoxSelecting_ = false;
						
						float minX = (std::min)(boxSelectStartPos_.x, boxSelectEndPos_.x);
						float maxX = (std::max)(boxSelectStartPos_.x, boxSelectEndPos_.x);
						float minY = (std::min)(boxSelectStartPos_.y, boxSelectEndPos_.y);
						float maxY = (std::max)(boxSelectStartPos_.y, boxSelectEndPos_.y);
						
						if (maxX - minX > 2.0f && maxY - minY > 2.0f) {
							if (this->player_) {
								float gw, gh;
								FlyCamera::GetGameViewSize(gw, gh);
								Matrix4x4 vpMat = debugFlyCamera_->GetViewProjectionMatrix();
								const Skeleton& playerSkeleton = this->player_->GetSkeleton();
								
								for (const auto& joint : playerSkeleton.joints) {
									Vector3 pos = {
										joint.skeletonSpaceMatrix.m[3][0],
										joint.skeletonSpaceMatrix.m[3][1],
										joint.skeletonSpaceMatrix.m[3][2]
									};
									
									if (this->player_->GetObject3d()) {
										Vector3 currentScale = this->player_->GetObject3d()->GetScale();
										Matrix4x4 worldMat = MyMath::MakeAffineMatrix(currentScale, this->player_->GetQuaternion(), this->player_->GetPosition());
										pos = MyMath::Transform(pos, worldMat);
									}
									
									Vector3 screenPos = MyMath::WorldToScreen(pos, vpMat, gw, gh);
									
									if (screenPos.z > 0.0f && screenPos.z < 1.0f) {
										if (screenPos.x >= minX && screenPos.x <= maxX &&
											screenPos.y >= minY && screenPos.y <= maxY) {
											simulationManager_->AddSelectedBoneName(joint.name);
										}
									}
								}
							}
						}
					} else {

						if (!s_clickedOnBone && !io.KeyShift) {
							ImVec2 dragDelta(io.MousePos.x - s_mouseDownPos.x, io.MousePos.y - s_mouseDownPos.y);
							if (std::abs(dragDelta.x) < 2.0f && std::abs(dragDelta.y) < 2.0f) {
								simulationManager_->ClearSelectedBones();
							}
						}
					}
					s_isDraggingBone = false;
				}

				if (isBoxSelecting_) {
					float minViewX, minViewY, maxViewX, maxViewY;
					if (FlyCamera::GetGameViewBounds(minViewX, minViewY, maxViewX, maxViewY)) {
						ImVec2 start(minViewX + boxSelectStartPos_.x, minViewY + boxSelectStartPos_.y);
						ImVec2 end(minViewX + boxSelectEndPos_.x, minViewY + boxSelectEndPos_.y);
						ImU32 colFill = IM_COL32(100, 150, 250, 80);
						ImU32 colBorder = IM_COL32(150, 200, 255, 200);
						ImGui::GetForegroundDrawList()->AddRectFilled(start, end, colFill);
						ImGui::GetForegroundDrawList()->AddRect(start, end, colBorder, 0.0f, 0, 1.5f);
					}
				}
			}
		}
	} else {
		if (player_) {
			if (IsTitleBackgroundMode()) {
				// 出撃中もタイトル時と同じ追従カメラを使い、地形と飛行方向を連続させる。
				// タイトル背景でも機体を見失わないよう、通常のプレイヤー追従カメラを使う。
				// 視野角だけを緩やかに変化させ、飛行演出の速度感は維持する。
				const float time = static_cast<float>(titleBackgroundFrame_) * (1.0f / 60.0f);
				player_->UpdateCamera(camera.get(), nullptr, false);
				camera->SetFovY(0.60f + std::sin(time * 0.55f) * 0.035f);
			} else {
				Vector3* targetPos = nullptr;
				Vector3 enemyPos;
				if (lockedEnemy_) {
					enemyPos = lockedEnemy_->GetPosition();
					targetPos = &enemyPos;
				}
				player_->UpdateCamera(camera.get(), targetPos);
			}
		}
		camera->Update();
	}

	if (isSelectedOnlyPreview) {
		for (auto &enemy : enemies_) {
			enemy->UpdateModel();
		}
		if (enemyBulletManager_) {
			enemyBulletManager_->UpdateModels();
		}
		for (auto &obstacle : obstacles_) {
			obstacle->Update();
		}
	}

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
	if (allowWeaponInput && player_ && !isGameOver_ && !isSpecialAttackActive_) {
		Input *input = Input::GetInstance();

		// 通常射撃（長押しで連射可能）
		const bool isNormalFirePushed = input->PushMouseButton(0) || input->PushAction(PlayerAction::NormalFire);
		if (isNormalFirePushed) {
			if (normalFireCooldownTimer_ <= 0) {
				Enemy* aimTarget = nullptr;
				if (lockedEnemy_ && lockOnManager_->IsLockedEnemyAlive()) {
					aimTarget = lockedEnemy_;
				} else if (aimAssistEnemy_) {
					aimTarget = aimAssistEnemy_;
				}
				if (missilePresetManager_->FirePlayerMissile(MissileType::Normal, aimTarget)) {
					normalFireCooldownTimer_ = 8; // 8フレームごとに連続発射
				}
			}
		} else {
			normalFireCooldownTimer_ = 0;
		}

		if (normalFireCooldownTimer_ > 0) {
			--normalFireCooldownTimer_;
		}


		// 右クリックはホーミングミサイル射撃 (全モード共通)
		if (input->TriggerMouseButton(1) || input->TriggerAction(PlayerAction::HomingFire)) {
			lockOnManager_->BeginMultiLock();
		}
		if (isMultiLockCharging_ && (input->PushMouseButton(1) || input->PushAction(PlayerAction::HomingFire))) {
			lockOnManager_->UpdateMultiLock(activeCamera);
		}
		if (isMultiLockCharging_ && !input->PushMouseButton(1) && !input->PushAction(PlayerAction::HomingFire)) {
			lockOnManager_->FireMultiLockMissiles();
		}
	} else if (isMultiLockCharging_) {
		lockOnManager_->CancelMultiLock();
	}

	// ==========================================

	// ==========================================
	std::vector<Vector3> hitPositions;
	std::vector<Vector3> destroyedPositions;
	if (updateSelectedMissiles) {
		if (missileManager_) {
			missileManager_->Update(activeCamera, enemies_, obstacles_, hitPositions, destroyedPositions);
		}

		if (explosionManager_ && !hitPositions.empty()) {
			explosionManager_->CreateHitEffects(hitPositions);
		}
		if (explosionManager_ && !destroyedPositions.empty()) {
			explosionManager_->CreateDestructionEffects(destroyedPositions);
		}

	} else {
		if (missileManager_) {
			missileManager_->UpdateModels(activeCamera);
		}
	}


	if (((shouldUpdateGame && !isSimulation) || updateSelectedMissiles || updateSelectedParticles ||
		(shouldUpdateGame && isFullFlowPreview)) && explosionManager_) {
		explosionManager_->Update();
	}


	if (!isSimulation || updateSelectedMissiles || updateSelectedParticles || (shouldUpdateGame && isFullFlowPreview)) {
	}

	// ==========================================

	// ==========================================
	if (showDebugColliders && updateDebugWireframes && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		std::vector<VertexData> colliderVertices;
		const bool drawAllDebugFrames = !isSelectedOnlyPreview || isFullFlowPreview;
		const bool drawPlayerDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 0;
		const bool drawMissileDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 1;
		const bool drawEnemyDebugFrame = drawAllDebugFrames || uiManager_->currentSimulationTarget_ == 2;
		const bool drawObstacleDebugFrame = drawAllDebugFrames;

		auto pushLine = [&](const Vector3& start, const Vector3& end, const Vector4& color) {
			VertexData v1{};
			VertexData v2{};
			v1.position = { start.x, start.y, start.z, 1.0f };
			v2.position = { end.x, end.y, end.z, 1.0f };
			v1.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			v2.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			v1.texcoord = { 0.0f, 0.0f, 0.0f, 0.0f };
			v2.texcoord = { 1.0f, 1.0f, 0.0f, 0.0f };
			v1.color = color;
			v2.color = color;
			colliderVertices.push_back(v1);
			colliderVertices.push_back(v2);
		};

		auto addAABB = [&](const Vector3& center, const Vector3& extents, const Vector4& color) {
			Vector3 p[8] = {
				{ center.x - extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z - extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z - extents.z },
				{ center.x - extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y - extents.y, center.z + extents.z },
				{ center.x + extents.x, center.y + extents.y, center.z + extents.z },
				{ center.x - extents.x, center.y + extents.y, center.z + extents.z },
			};

			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addOBB = [&](const Vector3& center, const Vector3& extents, const Vector3& rotation, const Vector4& color) {
			Vector3 local[8] = {
				{ -extents.x, -extents.y, -extents.z },
				{  extents.x, -extents.y, -extents.z },
				{  extents.x,  extents.y, -extents.z },
				{ -extents.x,  extents.y, -extents.z },
				{ -extents.x, -extents.y,  extents.z },
				{  extents.x, -extents.y,  extents.z },
				{  extents.x,  extents.y,  extents.z },
				{ -extents.x,  extents.y,  extents.z },
			};
			const Matrix4x4 rotationMatrix = MyMath::Multiply(
				MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation.x), MyMath::MakeRotateYMatrix(rotation.y)),
				MyMath::MakeRotateZMatrix(rotation.z));

			Vector3 p[8]{};
			for (int i = 0; i < 8; ++i) {
				const Vector3 rotated = MyMath::Transform(local[i], rotationMatrix);
				p[i] = AddVector3(center, rotated);
			}

			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addOBBShape = [&](const OBB& obb, const Vector4& color) {
			Vector3 p[8] = {
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
				AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
			};
			const int edges[12][2] = {
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};
			for (const auto& edge : edges) {
				pushLine(p[edge[0]], p[edge[1]], color);
			}
		};

		auto addSphere = [&](const Vector3& center, float radius, const Vector4& color) {
			constexpr int segmentCount = 24;
			constexpr float twoPi = 6.283185307f;
			for (int i = 0; i < segmentCount; ++i) {
				const float angle1 = twoPi * static_cast<float>(i) / static_cast<float>(segmentCount);
				const float angle2 = twoPi * static_cast<float>(i + 1) / static_cast<float>(segmentCount);
				const float cos1 = std::cos(angle1);
				const float sin1 = std::sin(angle1);
				const float cos2 = std::cos(angle2);
				const float sin2 = std::sin(angle2);

				pushLine(
					{ center.x + radius * cos1, center.y + radius * sin1, center.z },
					{ center.x + radius * cos2, center.y + radius * sin2, center.z },
					color);
				pushLine(
					{ center.x, center.y + radius * cos1, center.z + radius * sin1 },
					{ center.x, center.y + radius * cos2, center.z + radius * sin2 },
					color);
				pushLine(
					{ center.x + radius * sin1, center.y, center.z + radius * cos1 },
					{ center.x + radius * sin2, center.y, center.z + radius * cos2 },
					color);
			}
		};


		if (drawPlayerDebugFrame && player_ && !player_->IsDead()) {
			addOBBShape(player_->GetOBB(), { 0.0f, 1.0f, 0.0f, 1.0f });
		}


		if (drawObstacleDebugFrame) {
			for (const auto& obstacle : obstacles_) {
				if (!obstacle || obstacle->IsStageBounds()) {
					continue;
				}

				addOBBShape(obstacle->GetOBB(), { 0.0f, 1.0f, 1.0f, 1.0f });
			}
		}


		if (drawEnemyDebugFrame) {
			for (const auto& enemy : enemies_) {
				if (!enemy->IsDead()) {
					addOBBShape(enemy->GetOBB(), { 1.0f, 0.0f, 0.0f, 1.0f });
				}
			}
		}

		if ((drawEnemyDebugFrame || drawMissileDebugFrame) && lockedEnemy_ && !lockedEnemy_->IsDead()) {
			addSphere(lockedEnemy_->GetPosition(), lockedEnemy_->GetCollisionRadius() + 0.35f, { 1.0f, 0.95f, 0.0f, 1.0f });
		}


		if (drawMissileDebugFrame && missileManager_) {
			for (const auto& missile : missileManager_->GetMissiles()) {
				if (!missile->IsDead()) {
					// Sphere: Magenta (radius: 0.5f)
					addSphere(missile->GetPosition(), missile->GetCollisionRadius(), { 1.0f, 0.0f, 1.0f, 1.0f });
				}
			}
		}


		if (drawEnemyDebugFrame && enemyBulletManager_) {
			for (const auto& bullet : enemyBulletManager_->GetBullets()) {
				if (!bullet.isDead) {
					// Sphere: Orange (radius: 0.5f)
					addSphere(bullet.position, 0.5f, { 1.0f, 0.5f, 0.0f, 1.0f });
				}
			}
		}

		if (colliderVertices.empty()) {
			VertexData v1, v2;
			v1.position = { 0.0f, 0.0f, 0.0f, 1.0f };
			v1.color = { 0.0f, 0.0f, 0.0f, 0.0f };
			v2.position = { 0.0f, 0.0f, 0.0f, 1.0f };
			v2.color = { 0.0f, 0.0f, 0.0f, 0.0f };
			colliderVertices.push_back(v1);
			colliderVertices.push_back(v2);
		}

		debugColliderLinesObject->GetModel()->UpdateLineVertices(colliderVertices);
		debugColliderLinesObject->Update();
	}

	if (environmentRenderer_) {
		// 編集モード停止中は、パーティクル・UVスクロールを含む環境演出も止める。
		environmentRenderer_->Update(activeCamera, shouldUpdateGame);
	}

#ifdef ENABLE_IMGUI
	if (uiManager_) {
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
	// 深度を書き込まない炎やパーティクルを、背景で上書きしない。
	if (environmentRenderer_) {
		environmentRenderer_->DrawBackground();
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();


	if (player_) {
		// 機体・ノズルは不透明パス。炎は地形の深度が完成してから描く。
		if (isEditorPreviewPlaying_ && player_->GetBoosterEffect()) {
			// 編集UIを閉じて再生した際に位置確認用の黄色い球を残さない。
			player_->GetBoosterEffect()->SetPlacementMarkersVisible(false);
		}
		player_->Draw(renderCamera, false);
	}

	bool isAnimationEditor = IsSimulationMode() && uiManager_ && uiManager_->currentSimulationTarget_ == 5;
	if (isAnimationEditor) {
		if (player_) {
			player_->DrawEffects(renderCamera);
		}

		Object3dCommon::GetInstance()->SetCommonDrawSettings();
		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
		return;
	}


	if (missileManager_) {
		missileManager_->Draw();
	}


	if (enemyBulletManager_) {
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

	if (player_ && boundaryAlertObject_ && ceilingBoundaryAlertObject_) {
		static float pulseTime = 0.0f;
		pulseTime += 0.05f;
		float pulseAlpha = 0.5f + 0.5f * std::sin(pulseTime);

		auto drawBoundaryAlert = [&](Object3d* alertObject, const Vector3& position, const Vector3& normal, float intensity) {
			alertObject->SetScale({ 2.0f, 2.0f, 2.0f });

			// The plane model is already upright on the XY plane. Walls only need yaw;
			// the ceiling needs a pitch so the alert lies on the horizontal surface.
			Vector3 rotate = { 0.0f, std::atan2(normal.x, normal.z), 0.0f };
			if (normal.y > 0.5f) {
				rotate = { -1.570796f, 0.0f, 0.0f };
			}

			Model* m = alertObject->GetModel();
			if (m) {
				m->SetColor({ 1.0f, 1.0f, 1.0f, intensity * pulseAlpha });
			}

			alertObject->SetTranslate({
				position.x + normal.x * 0.5f,
				position.y + normal.y * 0.5f,
				position.z + normal.z * 0.5f
			});

			alertObject->SetRotate(rotate);
			alertObject->Update();
			alertObject->Draw();
		};

		if (player_->IsNearWallBoundary()) {
			drawBoundaryAlert(
				boundaryAlertObject_.get(),
				player_->GetWallBoundaryAlertPosition(),
				player_->GetWallBoundaryAlertNormal(),
				player_->GetWallBoundaryWarningIntensity());
		}
		if (player_->IsNearCeilingBoundary()) {
			drawBoundaryAlert(
				ceilingBoundaryAlertObject_.get(),
				player_->GetCeilingBoundaryAlertPosition(),
				player_->GetCeilingBoundaryAlertNormal(),
				player_->GetCeilingBoundaryWarningIntensity());
		}
		if (player_->IsNearBoundary()) {
			Object3dCommon::GetInstance()->SetCommonDrawSettings();
		}
	}
	
	if (showBones) {

		Object3dCommon::GetInstance()->SetCommonDrawSettings();


		if (skeletonLinesObject && skeletonLinesObject->GetModel()) {
			skeletonLinesObject->Draw();
		}
	}

	if (showDebugColliders && debugColliderLinesObject && debugColliderLinesObject->GetModel()) {
		debugColliderLinesObject->Draw();
	}
	

	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (environmentRenderer_) environmentRenderer_->Draw();

	// すべての不透明オブジェクトの後に、実際の噴射を加算する。
	// Z-Test は有効、Z-Write は無効のまま、前景の翼・地形には正しく隠れる。
	if (player_) {
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

void GamePlayScene::DrawOverlay() {
	if (isDebugCameraActive_ && !debugFlyCamera_) return;
	if (!isDebugCameraActive_ && !camera) return;

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (!activeCamera) return;

	if (IsTitleBackgroundMode()) {
		// タイトルでは戦闘 HUD を出さず、ロックオン表示だけを残す。
		if (lockedEnemy_ && !lockedEnemy_->IsDead()) {
			DrawLockOnOverlaySprite(lockedEnemy_, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get(), false, false);
		}
		return;
	}

	// HUD
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const float statusPanelX = 20.0f;
	// 左下は機体の生存・必殺技・歌をまとめた戦闘ステータスパネルにする。
	const float statusPanelY = screenHeight - 180.0f;
	const float ammoPanelX = screenWidth - 420.0f;
	const float ammoPanelY = screenHeight - 170.0f;
	if (hudPanelSprite_) {
		hudPanelSprite_->SetPosition({ statusPanelX, statusPanelY });
		hudPanelSprite_->SetSize({ 390.0f, 160.0f });
		hudPanelSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.92f });
		hudPanelSprite_->Update();
		hudPanelSprite_->Draw();
	}
	if (hudAmmoPanelSprite_) {
		hudAmmoPanelSprite_->SetPosition({ ammoPanelX, ammoPanelY });
		hudAmmoPanelSprite_->SetSize({ 400.0f, 150.0f });
		hudAmmoPanelSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.92f });
		hudAmmoPanelSprite_->Update();
		hudAmmoPanelSprite_->Draw();
	}

	auto drawLabel = [](Sprite *label, const Vector2 &position, const Vector2 &size) {
		if (!label) return;
		label->SetPosition(position);
		label->SetSize(size);
		label->Update();
		label->Draw();
	};
	drawLabel(hudHpLabelSprite_.get(), { statusPanelX + 30.0f, statusPanelY + 29.0f }, { 42.0f, 25.0f });
	drawLabel(hudSpLabelSprite_.get(), { statusPanelX + 30.0f, statusPanelY + 77.0f }, { 39.0f, 25.0f });
	drawLabel(hudAmmoLabelSprite_.get(), { ammoPanelX + 32.0f, ammoPanelY + 22.0f }, { 82.0f, 25.0f });

	auto drawText = [](const std::string &text, auto &sprites, float rightX, float y, const Vector4 &color) {
		constexpr float digitWidth = 22.0f;
		constexpr float digitHeight = 28.0f;
		const float startX = rightX - digitWidth * static_cast<float>(text.size());
		for (size_t index = 0; index < text.size() && index < sprites.size(); ++index) {
			Sprite *digitSprite = sprites[index].get();
			const int glyphIndex = (text[index] == '/') ? 10 : text[index] - '0';
			digitSprite->SetTextureLeftTop({ static_cast<float>(glyphIndex * 64), 0.0f });
			digitSprite->SetTextureSize({ 64.0f, 80.0f });
			digitSprite->SetPosition({ startX + digitWidth * static_cast<float>(index), y });
			digitSprite->SetSize({ digitWidth, digitHeight });
			digitSprite->SetColor(color);
			digitSprite->Update();
			digitSprite->Draw();
		}
	};
	const float statusGaugeX = statusPanelX + 88.0f;
	const float statusGaugeWidth = 265.0f;
	const float statusGaugeHeight = 18.0f;
	if (hpGaugeBackgroundSprite_ && hpGaugeFillSprite_) {
		hpGaugeBackgroundSprite_->SetPosition({ statusGaugeX - 2.0f, statusPanelY + 32.0f });
		hpGaugeBackgroundSprite_->SetSize({ statusGaugeWidth + 4.0f, statusGaugeHeight + 4.0f });
		hpGaugeBackgroundSprite_->SetColor({ 0.04f, 0.03f, 0.08f, 0.92f });
		hpGaugeBackgroundSprite_->Update();
		hpGaugeBackgroundSprite_->Draw();
		const float hpRatio = std::clamp(
			static_cast<float>(player_ ? player_->GetHP() : 0) / static_cast<float>(Player::kMaxHP),
			0.0f,
			1.0f);
		hpGaugeFillSprite_->SetPosition({ statusGaugeX, statusPanelY + 34.0f });
		hpGaugeFillSprite_->SetSize({ statusGaugeWidth * hpRatio, statusGaugeHeight });
		hpGaugeFillSprite_->SetColor({ 1.0f, 0.12f, 0.18f, 1.0f });
		hpGaugeFillSprite_->Update();
		hpGaugeFillSprite_->Draw();
	}

	// ボスが生存している間だけ、画面上部中央に専用HPバーを表示する。
	Boss* activeBoss = nullptr;
	for (const auto& enemy : enemies_) {
		if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
			activeBoss = static_cast<Boss*>(enemy.get());
			break;
		}
	}
	if (activeBoss && bossHpGaugeBackgroundSprite_ && bossHpGaugeFillSprite_) {
		const float bossGaugeWidth = (std::min)(560.0f, (std::max)(screenWidth - 80.0f, 160.0f));
		constexpr float kBossGaugeHeight = 22.0f;
		const float bossGaugeX = (screenWidth - bossGaugeWidth) * 0.5f;
		constexpr float kBossGaugeY = 42.0f;
		const float hpRatio = std::clamp(
			static_cast<float>(activeBoss->GetHP()) / static_cast<float>(Boss::kMaxHP),
			0.0f,
			1.0f);

		bossHpGaugeBackgroundSprite_->SetPosition({ bossGaugeX - 4.0f, kBossGaugeY - 4.0f });
		bossHpGaugeBackgroundSprite_->SetSize({ bossGaugeWidth + 8.0f, kBossGaugeHeight + 8.0f });
		bossHpGaugeBackgroundSprite_->SetColor({ 0.05f, 0.07f, 0.15f, 0.94f });
		bossHpGaugeBackgroundSprite_->Update();
		bossHpGaugeBackgroundSprite_->Draw();

		bossHpGaugeFillSprite_->SetPosition({ bossGaugeX, kBossGaugeY });
		bossHpGaugeFillSprite_->SetSize({ bossGaugeWidth * hpRatio, kBossGaugeHeight });
		bossHpGaugeFillSprite_->SetColor({ 1.0f, 0.18f, 0.10f, 1.0f });
		bossHpGaugeFillSprite_->Update();
		bossHpGaugeFillSprite_->Draw();
	}
	if (hudNormalAmmoIconSprite_ && hudHomingAmmoIconSprite_) {
		hudNormalAmmoIconSprite_->SetTextureLeftTop({ 0.0f, 0.0f });
		hudNormalAmmoIconSprite_->SetTextureSize({ 887.0f, 887.0f });
		hudNormalAmmoIconSprite_->SetPosition({ ammoPanelX + 34.0f, ammoPanelY + 53.0f });
		hudNormalAmmoIconSprite_->SetSize({ 48.0f, 48.0f });
		hudNormalAmmoIconSprite_->Update();
		hudNormalAmmoIconSprite_->Draw();
		hudHomingAmmoIconSprite_->SetTextureLeftTop({ 887.0f, 0.0f });
		hudHomingAmmoIconSprite_->SetTextureSize({ 887.0f, 887.0f });
		hudHomingAmmoIconSprite_->SetPosition({ ammoPanelX + 34.0f, ammoPanelY + 91.0f });
		hudHomingAmmoIconSprite_->SetSize({ 48.0f, 48.0f });
		hudHomingAmmoIconSprite_->Update();
		hudHomingAmmoIconSprite_->Draw();
	}
	const Vector4 normalAmmoColor = ammoManager_->IsNormalReloading()
		? Vector4{ 1.0f, 1.0f, 0.65f, 1.0f }
		: Vector4{ 1.0f, 0.78f, 0.08f, 1.0f };
	const Vector4 homingAmmoColor = ammoManager_->IsHomingReloading()
		? Vector4{ 1.0f, 0.65f, 0.65f, 1.0f }
		: Vector4{ 1.0f, 0.18f, 0.12f, 1.0f };
	drawText(std::to_string(ammoManager_->GetNormalMagazine()), hudNormalAmmoDigitSprites_, ammoPanelX + 365.0f, ammoPanelY + 60.0f, normalAmmoColor);
	drawText(std::to_string(ammoManager_->GetHomingMagazine()), hudHomingAmmoDigitSprites_, ammoPanelX + 365.0f, ammoPanelY + 98.0f, homingAmmoColor);
	if (ammoManager_->IsNormalReloading() && hudNormalReloadGaugeSprite_) {
		hudNormalReloadGaugeSprite_->SetPosition({ ammoPanelX + 90.0f, ammoPanelY + 89.0f });
		hudNormalReloadGaugeSprite_->SetSize({ 275.0f * static_cast<float>(ammoManager_->GetNormalReloadFrame()) / static_cast<float>(AmmoManager::kReloadDurationFrames), 4.0f });
		hudNormalReloadGaugeSprite_->SetColor({ 1.0f, 0.78f, 0.08f, 1.0f });
		hudNormalReloadGaugeSprite_->Update();
		hudNormalReloadGaugeSprite_->Draw();
	}
	if (ammoManager_->IsHomingReloading() && hudHomingReloadGaugeSprite_) {
		hudHomingReloadGaugeSprite_->SetPosition({ ammoPanelX + 90.0f, ammoPanelY + 127.0f });
		hudHomingReloadGaugeSprite_->SetSize({ 275.0f * static_cast<float>(ammoManager_->GetHomingReloadFrame()) / static_cast<float>(AmmoManager::kReloadDurationFrames), 4.0f });
		hudHomingReloadGaugeSprite_->SetColor({ 1.0f, 0.18f, 0.12f, 1.0f });
		hudHomingReloadGaugeSprite_->Update();
		hudHomingReloadGaugeSprite_->Draw();
	}

	// gauge
	const float gaugeX = statusGaugeX;
	const float gaugeY = statusPanelY + 82.0f;
	const float gaugeWidth = statusGaugeWidth;
	const float gaugeHeight = 18.0f;
	if (spGaugeBackgroundSprite_ && spGaugeFillSprite_ && spGaugeCostMarkerSprite_) {
		spGaugeBackgroundSprite_->SetPosition({ gaugeX - 2.0f, gaugeY - 2.0f });
		spGaugeBackgroundSprite_->SetSize({ gaugeWidth + 4.0f, gaugeHeight + 4.0f });
		spGaugeBackgroundSprite_->SetColor({ 0.03f, 0.05f, 0.12f, 0.90f });
		spGaugeBackgroundSprite_->Update();
		spGaugeBackgroundSprite_->Draw();

		spGaugeFillSprite_->SetPosition({ gaugeX, gaugeY });
		spGaugeFillSprite_->SetSize({ gaugeWidth * std::clamp(spGauge_ / 100.0f, 0.0f, 1.0f), gaugeHeight });
		spGaugeFillSprite_->SetColor(isSpecialAttackActive_
			? Vector4{ 1.0f, 0.35f, 0.05f, 1.0f }
			: Vector4{ 0.05f, 0.75f, 1.0f, 1.0f });
		spGaugeFillSprite_->Update();
		spGaugeFillSprite_->Draw();

		spGaugeCostMarkerSprite_->SetPosition({ gaugeX + gaugeWidth * 0.5f - 1.0f, gaugeY });
		spGaugeCostMarkerSprite_->SetSize({ 2.0f, gaugeHeight });
		spGaugeCostMarkerSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.9f });
		spGaugeCostMarkerSprite_->Update();

	constexpr float kRadarRange = 250.0f;
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
		spGaugeCostMarkerSprite_->Draw();
	}

	// 歌ゲージは、蓄積中は撃破で得た残量、発動中は15秒の残り時間を表示する。
	// これにより、歌発動の瞬間に空になって「効果時間が分からない」状態を避ける。
	const float songGaugeY = statusPanelY + 122.0f;
	// 状態表示用の余白を右に残し、READY / LIVE がパネルの外へ出ないようにする。
	const float songGaugeWidth = gaugeWidth - 40.0f;
	constexpr int kSongDurationFrames = 900;
	const float songRatio = isSongActive_
		? std::clamp(1.0f - static_cast<float>(songFrame_) / static_cast<float>(kSongDurationFrames), 0.0f, 1.0f)
		: std::clamp(songGauge_ / 100.0f, 0.0f, 1.0f);
	if (songGaugeBackgroundSprite_ && songGaugeFillSprite_ && songGaugeStateSprite_) {
		songGaugeBackgroundSprite_->SetPosition({ gaugeX - 2.0f, songGaugeY - 2.0f });
		songGaugeBackgroundSprite_->SetSize({ songGaugeWidth + 4.0f, gaugeHeight + 4.0f });
		songGaugeBackgroundSprite_->SetColor({ 0.06f, 0.02f, 0.12f, 0.94f });
		songGaugeBackgroundSprite_->Update();
		songGaugeBackgroundSprite_->Draw();

		const float songPulse = isSongActive_ ? 0.72f + 0.28f * std::sin(static_cast<float>(songFrame_) * 0.24f) : 1.0f;
		songGaugeFillSprite_->SetPosition({ gaugeX, songGaugeY });
		songGaugeFillSprite_->SetSize({ songGaugeWidth * songRatio, gaugeHeight });
		songGaugeFillSprite_->SetColor(isSongActive_
			? Vector4{ 1.0f, 0.16f, 0.76f, songPulse }
			: (songGauge_ >= 100.0f
				? Vector4{ 0.42f, 1.0f, 0.30f, 1.0f }
				: Vector4{ 0.08f, 0.85f, 0.92f, 1.0f }));
		songGaugeFillSprite_->Update();
		songGaugeFillSprite_->Draw();

		// 右端の発光インジケータは READY / LIVE / CHARGE の状態を一目で区別させる。
		songGaugeStateSprite_->SetPosition({ gaugeX + songGaugeWidth + 6.0f, songGaugeY + 2.0f });
		songGaugeStateSprite_->SetSize({ 14.0f, 14.0f });
		songGaugeStateSprite_->SetColor(isSongActive_
			? Vector4{ 1.0f, 0.22f, 0.78f, songPulse }
			: (songGauge_ >= 100.0f
				? Vector4{ 0.42f, 1.0f, 0.30f, 1.0f }
				: Vector4{ 0.10f, 0.55f, 0.72f, 0.9f }));
		songGaugeStateSprite_->Update();
		songGaugeStateSprite_->Draw();
	}

	// ImGui の有無に関係なく、実行ファイルのゲーム画面にも操作方法を出す。
	if (!isPaused_ && !isGameOver_ && GameSettings::GetInstance().IsControlGuideVisible() && controlsGuideSprite_) {
		controlsGuideSprite_->SetPosition({ 18.0f, 18.0f });
		controlsGuideSprite_->SetSize({ 320.0f, 213.0f });
		controlsGuideSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f });
		controlsGuideSprite_->Update();
		controlsGuideSprite_->Draw();
	}

#ifdef ENABLE_IMGUI
	// ImGuiの座標はアプリ全体、Spriteの座標はGame View内部なので、
	// Game Viewの矩形へ変換してHUDの外へ文字がずれないようにする。
	float gameViewMinX = 0.0f;
	float gameViewMinY = 0.0f;
	float gameViewMaxX = screenWidth;
	float gameViewMaxY = screenHeight;
	if (ImGui::GetCurrentContext() &&
		FlyCamera::GetGameViewBounds(gameViewMinX, gameViewMinY, gameViewMaxX, gameViewMaxY)) {
		ImDrawList *hudText = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
		const float viewScaleX = (gameViewMaxX - gameViewMinX) / (std::max)(screenWidth, 1.0f);
		const float viewScaleY = (gameViewMaxY - gameViewMinY) / (std::max)(screenHeight, 1.0f);
		auto toGameView = [=](float x, float y) {
			return ImVec2(gameViewMinX + x * viewScaleX, gameViewMinY + y * viewScaleY);
		};
		const ImU32 songTextColor = isSongActive_
			? IM_COL32(255, 95, 215, 255)
			: (songGauge_ >= 100.0f ? IM_COL32(125, 255, 125, 255) : IM_COL32(105, 225, 240, 255));
		hudText->AddText(toGameView(statusPanelX + 30.0f, songGaugeY + 1.0f), songTextColor, "SONG");
		const char *songState = isSongActive_ ? "LIVE" : (songGauge_ >= 100.0f ? "READY" : "CHG");
		hudText->AddText(toGameView(gaugeX + songGaugeWidth + 26.0f, songGaugeY + 1.0f), songTextColor, songState);

		// 常時確認できる操作ガイド。レーダーと重ならない左上に置き、
		// 操作名とキーを列で分けて、戦闘中でも一目で確認できるようにする。
		if (false && !isPaused_ && !isGameOver_ && GameSettings::GetInstance().IsControlGuideVisible()) {
			constexpr float kGuideX = 24.0f;
			constexpr float kGuideY = 24.0f;
			constexpr float kGuideWidth = 400.0f;
			constexpr float kGuideHeight = 238.0f;
			hudText->AddRectFilled(
				toGameView(kGuideX, kGuideY),
				toGameView(kGuideX + kGuideWidth, kGuideY + kGuideHeight),
				IM_COL32(3, 14, 30, 202),
				8.0f);
			hudText->AddRect(
				toGameView(kGuideX, kGuideY),
				toGameView(kGuideX + kGuideWidth, kGuideY + kGuideHeight),
				IM_COL32(60, 205, 245, 210),
				8.0f);
			hudText->AddRectFilled(
				toGameView(kGuideX + 1.0f, kGuideY + 1.0f),
				toGameView(kGuideX + kGuideWidth - 1.0f, kGuideY + 36.0f),
				IM_COL32(8, 54, 84, 210),
				8.0f);
			hudText->AddLine(
				toGameView(kGuideX + 12.0f, kGuideY + 42.0f),
				toGameView(kGuideX + kGuideWidth - 12.0f, kGuideY + 42.0f),
				IM_COL32(55, 175, 215, 180),
				1.0f);

			const PlayerMode playerMode = player_ ? player_->GetCurrentMode() : PlayerMode::Fighter;
			const char *modeName = playerMode == PlayerMode::Fighter ? "FIGHTER"
				: (playerMode == PlayerMode::Gerwalk ? "GERWALK" : "BATTROID");
			const char *lateralKey = playerMode == PlayerMode::Gerwalk ? "A / D  STRAFE"
				: (playerMode == PlayerMode::Fighter ? "A / D  DODGE" : "-" );
			const ImU32 guideTitleColor = IM_COL32(125, 236, 255, 255);
			const ImU32 guideModeColor = IM_COL32(255, 218, 105, 255);
			const ImU32 guideLabelColor = IM_COL32(158, 207, 226, 245);
			const ImU32 guideKeyColor = IM_COL32(244, 250, 255, 255);
			constexpr float kGuideFontSize = 14.0f;
			hudText->AddText(nullptr, 16.0f, toGameView(kGuideX + 14.0f, kGuideY + 10.0f), guideTitleColor, "CONTROLS");
			const ImVec2 modeSize = ImGui::CalcTextSize(modeName);
			hudText->AddText(nullptr, kGuideFontSize, toGameView(kGuideX + kGuideWidth - 18.0f - modeSize.x, kGuideY + 12.0f), guideModeColor, modeName);

			const auto drawGuideRow = [&](float y, const char *label, const char *key) {
				hudText->AddText(nullptr, kGuideFontSize, toGameView(kGuideX + 16.0f, y), guideLabelColor, label);
				hudText->AddText(nullptr, kGuideFontSize, toGameView(kGuideX + 170.0f, y), guideKeyColor, key);
			};
			drawGuideRow(kGuideY + 54.0f, "MOVE", "W / S");
			drawGuideRow(kGuideY + 76.0f, "UP / DOWN", "SPACE / SHIFT");
			drawGuideRow(kGuideY + 98.0f, "LATERAL", lateralKey);
			drawGuideRow(kGuideY + 124.0f, "GUN FIRE", "LEFT MOUSE");
			drawGuideRow(kGuideY + 146.0f, "MISSILE LOCK", "HOLD RIGHT MOUSE");
			drawGuideRow(kGuideY + 168.0f, "LOCK / RELEASE", "TAB / X");
			drawGuideRow(kGuideY + 190.0f, "TRANSFORM", "1 / 2 / 3");
			drawGuideRow(kGuideY + 212.0f, "PAUSE", "ESC");
		}
	}
#endif

	DrawRadar();

	bool isJammed = lockOnManager_->IsPlayerJammed(activeCamera);

	// 黄色のターゲットマーカー (単体ロックオン / エイムアシスト時)
	if (Enemy *overlayTarget = lockedEnemy_ ? lockedEnemy_ : aimAssistEnemy_) {
		if (!overlayTarget->IsDead()) {
			const bool isFighterLockDanger = lockedEnemy_ && lockOnManager_->IsFighterLockDanger();
			DrawLockOnOverlaySprite(overlayTarget, activeCamera->GetViewProjectionMatrix(), lockOnReticleSprite_.get(), isJammed, isFighterLockDanger);
		}
	} else {
		DrawAimCursorOverlaySprite(aimCursorSprite_.get(), isJammed);
	}

	// 赤色のターゲットマーカー：誘導弾操作中 (右クリック長押しマルチロック中) または 飛行中ミサイルが存在する時のみ表示
	std::vector<Enemy*> redMarkerTargets;
	if (isMultiLockCharging_) {
		for (Enemy* target : multiLockTargets_) {
			if (target && !target->IsDead()) {
				redMarkerTargets.push_back(target);
			}
		}
		if (!redMarkerTargets.empty()) {
			const int targetCount = static_cast<int>(redMarkerTargets.size());
			const int displayCount = (std::min)(targetCount, static_cast<int>(multiLockMarkerSprites_.size()));
			for (int i = 0; i < displayCount; ++i) {
				Enemy *target = redMarkerTargets[i];
				DrawIndividualMissileLockOnOverlaySprite(target, activeCamera->GetViewProjectionMatrix(), multiLockMarkerSprites_[static_cast<size_t>(i)].get(), isJammed, i + 1, displayCount);
			}
		}
	} else if (missileManager_) {
		for (const auto& missile : missileManager_->GetMissiles()) {
			if (missile && !missile->IsDead()) {
				Enemy* t = missile->GetTarget();
				if (t && !t->IsDead()) {
					if (std::find(redMarkerTargets.begin(), redMarkerTargets.end(), t) == redMarkerTargets.end()) {
						redMarkerTargets.push_back(t);
					}
				}
			}
		}
		if (!redMarkerTargets.empty()) {
			const int totalCount = (std::min)(static_cast<int>(redMarkerTargets.size()), static_cast<int>(multiLockMarkerSprites_.size()));
			for (int i = 0; i < totalCount; ++i) {
				Enemy *target = redMarkerTargets[i];
				DrawIndividualMissileLockOnOverlaySprite(target, activeCamera->GetViewProjectionMatrix(), multiLockMarkerSprites_[static_cast<size_t>(i)].get(), isJammed, i + 1, totalCount);
			}
		}
	}
	DrawPauseOverlay(screenWidth, screenHeight);
}

void GamePlayScene::DrawPauseOverlay(float screenWidth, float screenHeight) {
	if (!isPaused_ || !pauseOverlaySprite_ || !pausePanelSprite_) {
		return;
	}

	pauseOverlaySprite_->SetPosition({ 0.0f, 0.0f });
	pauseOverlaySprite_->SetSize({ screenWidth, screenHeight });
	pauseOverlaySprite_->SetColor({ 0.01f, 0.02f, 0.06f, 0.70f });
	pauseOverlaySprite_->Update();
	pauseOverlaySprite_->Draw();

	const float panelWidth = (std::min)(460.0f, screenWidth - 48.0f);
	constexpr float kPanelHeight = 324.0f;
	const float panelX = (screenWidth - panelWidth) * 0.5f;
	const float panelY = (screenHeight - kPanelHeight) * 0.5f;
	pausePanelSprite_->SetPosition({ panelX, panelY });
	pausePanelSprite_->SetSize({ panelWidth, kPanelHeight });
	pausePanelSprite_->SetColor({ 0.04f, 0.12f, 0.24f, 0.96f });
	pausePanelSprite_->Update();
	pausePanelSprite_->Draw();
	pausePanelSprite_->SetPosition({ panelX, panelY + 8.0f });
	pausePanelSprite_->SetSize({ panelWidth, 4.0f });
	pausePanelSprite_->SetColor({ 0.05f, 0.82f, 1.0f, 1.0f });
	pausePanelSprite_->Update();
	pausePanelSprite_->Draw();

	// メニューの選択状態は、文字の周囲を発光枠で示す。画像の文字を隠さないため、
	// 横線と左右のマーカーだけを最後に重ねる。
	const float labelsX = panelX + 17.0f;
	const float labelsY = panelY + 18.0f;
	const float labelsWidth = panelWidth - 34.0f;
	const float labelsHeight = labelsWidth * (2.0f / 3.0f);
	const float selectedY = labelsY + (isPauseTitleSelected_ ? 181.0f : 135.0f);
	const Vector4 selectedColor = { 0.08f, 0.90f, 1.0f, 0.95f };
	const Vector4 selectedGlowColor = { 0.05f, 0.40f, 0.80f, 0.78f };
	auto drawPauseSelection = [&](float y, const Vector4& color, const Vector2& size) {
		pausePanelSprite_->SetPosition({ labelsX + 18.0f, y });
		pausePanelSprite_->SetSize(size);
		pausePanelSprite_->SetColor(color);
		pausePanelSprite_->Update();
		pausePanelSprite_->Draw();
	};
	drawPauseSelection(selectedY, selectedGlowColor, { labelsWidth - 36.0f, 3.0f });
	drawPauseSelection(selectedY + 40.0f, selectedColor, { labelsWidth - 36.0f, 3.0f });
	drawPauseSelection(selectedY, selectedColor, { 4.0f, 43.0f });
	drawPauseSelection(selectedY, selectedColor, { 36.0f, 4.0f });
	drawPauseSelection(selectedY + 39.0f, selectedColor, { 36.0f, 4.0f });

	if (pauseMenuLabelsSprite_) {
		pauseMenuLabelsSprite_->SetPosition({ labelsX, labelsY });
		pauseMenuLabelsSprite_->SetSize({ labelsWidth, labelsHeight });
		pauseMenuLabelsSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		pauseMenuLabelsSprite_->Update();
		pauseMenuLabelsSprite_->Draw();
	}

	// 文字より前面に小さな発光マーカーを置き、選択中の項目を常に判別可能にする。
	drawPauseSelection(selectedY + 13.0f, selectedColor, { 9.0f, 17.0f });

#ifdef ENABLE_IMGUI
	// 通常プレイでは ImGui のフレームを開始していない。コンテキストが残っていても
	// フォントの描画準備は済んでいないため、非表示中に AddText / CalcTextSize を
	// 呼ぶと ImGui 内部でアクセス違反になる。
	if (false && ImGuiManager::IsVisible() && ImGui::GetCurrentContext()) {
		ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
		const char* title = "PAUSED";
		const char* guide = "UP / DOWN : SELECT     ENTER : CONFIRM";
		const char* resume = isPauseTitleSelected_ ? "  RESUME GAME" : "> RESUME GAME";
		const char* returnToTitle = isPauseTitleSelected_ ? "> RETURN TO TITLE" : "  RETURN TO TITLE";
		const ImVec2 titleSize = ImGui::CalcTextSize(title);
		const ImVec2 guideSize = ImGui::CalcTextSize(guide);
		drawList->AddText(ImVec2((screenWidth - titleSize.x) * 0.5f, panelY + 30.0f), IM_COL32(120, 230, 255, 255), title);
		drawList->AddText(ImVec2((screenWidth - guideSize.x) * 0.5f, panelY + 62.0f), IM_COL32(185, 220, 240, 255), guide);

		const float optionX = panelX + 58.0f;
		const float optionWidth = panelWidth - 116.0f;
		const float resumeY = panelY + 104.0f;
		const float titleY = panelY + 150.0f;
		const ImU32 selectedColor = IM_COL32(12, 115, 170, 225);
		const ImU32 unselectedColor = IM_COL32(5, 28, 55, 180);
		drawList->AddRectFilled(ImVec2(optionX, resumeY), ImVec2(optionX + optionWidth, resumeY + 32.0f),
			isPauseTitleSelected_ ? unselectedColor : selectedColor, 4.0f);
		drawList->AddRectFilled(ImVec2(optionX, titleY), ImVec2(optionX + optionWidth, titleY + 32.0f),
			isPauseTitleSelected_ ? selectedColor : unselectedColor, 4.0f);
		drawList->AddText(ImVec2(optionX + 14.0f, resumeY + 8.0f),
			isPauseTitleSelected_ ? IM_COL32(205, 220, 230, 230) : IM_COL32(255, 255, 255, 255), resume);
		drawList->AddText(ImVec2(optionX + 14.0f, titleY + 8.0f),
			isPauseTitleSelected_ ? IM_COL32(255, 255, 255, 255) : IM_COL32(205, 220, 230, 230), returnToTitle);
		drawList->AddText(ImVec2((screenWidth - ImGui::CalcTextSize("ESC : RESUME").x) * 0.5f, panelY + 198.0f),
			IM_COL32(185, 220, 240, 255), "ESC : RESUME");
	}
#endif
}

void GamePlayScene::DrawRadar() {
	if (!player_ || !radarFrameSprite_) {
		return;
	}

	constexpr float kRadarRange = 250.0f;
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const float radarSize = (std::min)(230.0f, screenHeight * 0.30f);
	const float radarRadius = radarSize * 0.39f;
	const Vector2 radarCenter = {
		screenWidth - radarSize * 0.5f - 20.0f,
		radarSize * 0.5f + 20.0f
	};

	radarFrameSprite_->SetPosition(radarCenter);
	radarFrameSprite_->SetSize({ radarSize, radarSize });
	radarFrameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f });
	radarFrameSprite_->Update();
	radarFrameSprite_->Draw();

	radarSweepAngle_ += 0.025f;
	if (radarSweepAngle_ >= 6.2831853f) {
		radarSweepAngle_ -= 6.2831853f;
	}
	if (radarSweepSprite_) {
		radarSweepSprite_->SetPosition(radarCenter);
		radarSweepSprite_->SetSize({ radarRadius, 2.0f });
		radarSweepSprite_->SetRotation(radarSweepAngle_ - 1.5707963f);
		radarSweepSprite_->SetColor({ 0.0f, 0.9f, 1.0f, 0.48f });
		radarSweepSprite_->Update();
		radarSweepSprite_->Draw();
	}

	const Vector3 playerPosition = player_->GetPosition();
	const Vector3 playerForward = player_->GetForwardVector();
	const float forwardLength = std::sqrt(
		playerForward.x * playerForward.x + playerForward.z * playerForward.z);
	const float forwardX = forwardLength > 0.0001f ? playerForward.x / forwardLength : 0.0f;
	const float forwardZ = forwardLength > 0.0001f ? playerForward.z / forwardLength : 1.0f;
	const float rightX = forwardZ;
	const float rightZ = -forwardX;

	size_t blipIndex = 0;
	for (const auto &enemy : enemies_) {
		if (!enemy || enemy->IsDead() || blipIndex >= radarBlipSprites_.size()) {
			continue;
		}

		const Vector3 enemyPosition = enemy->GetPosition();
		const float deltaX = enemyPosition.x - playerPosition.x;
		const float deltaZ = enemyPosition.z - playerPosition.z;
		const float localRight = deltaX * rightX + deltaZ * rightZ;
		const float localForward = deltaX * forwardX + deltaZ * forwardZ;
		const float distance = std::sqrt(localRight * localRight + localForward * localForward);
		const float positionScale = distance > kRadarRange && distance > 0.0001f
			? kRadarRange / distance
			: 1.0f;

		Sprite *blip = radarBlipSprites_[blipIndex++].get();
		blip->SetPosition({
			radarCenter.x + (localRight * positionScale / kRadarRange) * radarRadius,
			radarCenter.y - (localForward * positionScale / kRadarRange) * radarRadius
		});
		const float blipSize = enemy->IsBoss() ? 12.0f : (enemy.get() == lockedEnemy_ ? 9.0f : 6.0f);
		blip->SetSize({ blipSize, blipSize });
		if (enemy.get() == lockedEnemy_) {
			blip->SetColor({ 1.0f, 0.95f, 0.05f, 1.0f });
		} else if (enemy->IsBoss()) {
			blip->SetColor({ 1.0f, 0.12f, 0.05f, 1.0f });
		} else {
			blip->SetColor({ 1.0f, 0.38f, 0.05f, 1.0f });
		}
		blip->Update();
		blip->Draw();
	}
}
