#include "GamePlayScene.h"
#include "GamePlaySceneResourceInitializer.h"
#include "GameplayWorldRenderer.h"
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
	mode_ = Mode::Gameplay;
	sessionFlowController_.ActivateFromWarp({
		camera.get(), player_.get(), &obstacles_,
		&titleLaunchState_, &titleBackgroundEnemyState_, &introBoostState_,
		&gameOverSequence_, &combatAlertState_, &damageFeedbackState_,
		&bossClearSequence_, &pauseMenuState_,
		&tutorialController_, &tutorialProgress_, &tutorialGoalMarker_,
		&defeatedEnemyCount_, &gameplayElapsedFrames_, &isEditorPreviewPlaying_, &warpFadeInAlpha_,
		[this]() { ReloadSceneJson(); },
		[this]() { ApplyAudioSettings(); },
	});
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
	overlayCoordinator_.Initialize();
	warpFadeInAlpha_ = 1.0f;
	TextureManager::GetInstance()->LoadTexture("resources/gameplay_controls.png");
	controlsGuideSprite_ = std::make_unique<Sprite>();
	controlsGuideSprite_->Initialize(SpriteCommon::GetInstance(), "resources/gameplay_controls.png");
	specialAttackState_.Reset();
	songState_.Reset();
	playerWeaponController_.Reset();
	tutorialController_.Reset(false);
	tutorialProgress_.Reset();
	ammoManager_->Initialize();

	boundaryShieldRenderer_.Initialize();

	GamePlaySceneResourceInitializer::InitializeBaseModelsAndPrimitives({
		&myShere,
		&myBox,
		&myModelObject,
		&animationData,
		&skeleton,
		&skeletonLinesObject,
		&missileTrail,
		&trailObject,
	});
	debugColliderRenderer_.Initialize();
	GamePlaySceneResourceInitializer::InitializeDebugCamera(debugFlyCamera_, isDebugCameraActive_);
	GamePlaySceneResourceInitializer::InitializeParticleGroups(
		environmentRenderer_->GetParticleManager(), kSongParticleGroup, kSpeedRushParticleGroup);

	audioController_.Initialize(IsTitleBackgroundMode(), songState_.IsActive());

	// 歌の残像はノズルの炎と共有せず、胴体と両翼端の飛行履歴を別々に保持する。
	songFlightTrailRenderer_.Initialize();
	bossClearSequence_.Initialize();
	enemyDefeatEffectController_.InitializeModel();

	GamePlaySceneResourceInitializer::InitializeCombatManagers({
		&player_,
		&missileManager_,
		&explosionManager_,
		&enemyBulletManager_,
		&cameraManager_,
	}, kPlayerModelName, environmentRenderer_->GetParticleManager());
	boundaryShieldSettings_.Apply(player_.get());
	CaptureGameplayBalanceDefaults();
	LoadGameplayBalanceSettings();
	ApplyGameplayBalanceSettings();
	enemies_.clear();

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
	sessionFlowController_.ApplyInitialStageTransition({
		IsAutoFlightMode(), IsAttractDemoMode(), player_.get(),
		&enemies_, &enemySpawns_, &enemySpawnManager_, &bossEncounterState_,
		&tutorialController_, &tutorialProgress_, &tutorialGoalMarker_,
		&warpExitPresentation_, &introBoostState_,
		[this]() { ClearAllEnemyReferences(); },
	});
	if (IsAttractDemoMode() && player_) {
		// タイトル背景の周回軌道ではなく、通常ステージに置かれたPLAYERの
		// 進行方向を使う。敵構成と攻撃もゲームプレイのものをそのまま使う。
		attractDemoController_.InitializeFlightDirection(player_->GetForwardVector());
	}

	warpExitPresentation_.InitializeGateObjects();
	GamePlaySceneResourceInitializer::PrimeStageStart(player_.get(), camera.get(), obstacles_);
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
	enemyDefeatEffectController_.Clear();
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
	sessionFlowController_.ResetEditorPreview({
		kPlayerModelName,
		isDebugCameraActive_,
		&isEditorPreviewPlaying_,
		camera.get(),
		player_.get(),
		environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr,
		missileManager_.get(),
		enemyBulletManager_.get(),
		explosionManager_.get(),
		lockOnManager_.get(),
		&targetSelection_,
		&tutorialController_,
		&gameOverSequence_,
		&combatAlertState_,
		&damageFeedbackState_,
		&bossClearSequence_,
		&enemyDefeatEffectController_,
		[this]() { ClearAllEnemyReferences(); },
		[this]() { ReloadSceneJson(); },
	});
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
	audioController_.Finalize();

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
	sessionFlowController_.CancelPauseForCinematic(pauseMenuState_, songState_.IsActive(), audioController_);
}

bool GamePlayScene::UpdatePauseMenu(bool canUseKeyboardInput) {
	return sessionFlowController_.UpdatePauseMenu({
		IsPauseBlockedByCinematic(),
		canUseKeyboardInput,
		songState_.IsActive(),
		&pauseMenuState_,
		&audioController_,
		&returnToTitleTransition_,
	});
}

void GamePlayScene::ApplyAudioSettings() {
	audioController_.ApplySettings(IsTitleBackgroundMode(), songState_.IsActive());
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
			SceneManager::GetInstance()->SetMissionResult({ MissionResultOutcome::Clear, defeatedEnemyCount_, player_ ? player_->GetHP() : 0, gameplayElapsedFrames_,
				IsAttractDemoMode() });
			SceneManager::GetInstance()->ChangeScene("CLEAR");
			return;
		}
	}

	bool shouldUpdateGame = !pauseMenuState_.IsPaused() && !isBossClearSequenceActive;
	if (updateCoordinator_.UpdateGameOverPhase({
			IsTitleBackgroundMode(),
			IsAttractDemoMode(),
			attractDemoController_.IsClearCinematic(),
			isBossClearSequenceActive,
			defeatedEnemyCount_,
			&gameplayElapsedFrames_,
			player_.get(),
			explosionManager_.get(),
			environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr,
			&audioController_,
			&gameOverSequence_,
			&tutorialController_,
			&playerFlightEffectController_,
			&damageFeedbackState_,
			[this]() { CancelPauseForCinematic(); },
		}, shouldUpdateGame)) {
		SceneManager::GetInstance()->ChangeScene("GAMEOVER");
	}

	const bool isMouseLookActive = gameplayModeController_.ShouldClipMouseCursor(
		runMode, gameOverSequence_.IsActive(), returnToTitleTransition_.IsActive(),
		shouldUpdateGame, canUsePlayerInput, ImGuiManager::IsVisible());
	Input::GetInstance()->SetMouseCursorClipEnabled(isMouseLookActive);

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

	ParticleManager* particleManager = environmentRenderer_ ? environmentRenderer_->GetParticleManager() : nullptr;

	playerWeaponController_.UpdateSpecialAndSong({
		shouldUpdateGame, canUsePlayerInput, gameOverSequence_.IsActive(),
		&gameplayBalanceSettings_.Get(), player_.get(), &enemies_, &targetSelection_,
		ammoManager_.get(), missilePresetManager_.get(), particleManager,
		&songFlightTrailRenderer_, &specialAttackState_, &songState_,
		audioController_.GetBgmVoice(), audioController_.GetSongVoice(),
	});

	updateCoordinator_.UpdateDebugAnimationAndBones({
		playAnimation, updateAnimationPreview, enableSkinning, showBones, showModel, modelScale,
		&animationTime, myBox.get(), myModelObject.get(), skeletonLinesObject.get(),
		&animationData, &skeleton, player_.get(), simulationManager_.get(),
	});

	activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	lockOnManager_->UpdateLockOn(activeCamera, allowLockOnBehavior);

	gameplayModeController_.UpdatePlayerForMode({
		runMode, updateSelectedPlayer, canUseKeyboardInput, shouldUpdateGame, isEditorPreviewPlaying_,
		player_.get(), &obstacles_, targetSelection_.GetLockedEnemy(), lockOnManager_.get(),
		&titleLaunchState_, &attractDemoController_, &warpExitPresentation_, &introBoostState_, particleManager,
	});

	if (updateCoordinator_.UpdateEnemyAndWavePhase({
			updateSelectedEnemies, IsAutoFlightMode(), gameOverSequence_.IsActive(),
			gameplayBalanceSettings_.Get().bossMaxActiveMinions,
			gameplayModeController_.GetTitleBackgroundFrame(),
			camera.get(), player_.get(), &enemies_, &obstacles_, &enemySpawns_,
			enemyBulletManager_.get(), explosionManager_.get(), particleManager,
			&combatAlertState_, &enemyEventManager_, &enemySpawnManager_,
			&enemyUpdateCoordinator_, &enemyDefeatEffectController_,
			&songState_, &bossClearSequence_, &bossEncounterState_, &targetSelection_,
			&titleBackgroundEnemyState_, &titleBackgroundEnemyController_,
			&titleBackgroundEnemyFlightController_, &titleBackgroundEnemyWeaponController_,
			missilePresetManager_.get(), uiManager_.get(), &defeatedEnemyCount_,
			[this](Enemy* enemy) { ClearEnemyReferences(enemy); },
		})) {
		SceneManager::GetInstance()->ChangeScene("CLEAR");
		return;
	}

	debugEditorController_.UpdateBossEditorPartSelection(
		isDebugCameraActive_ ? static_cast<Camera*>(debugFlyCamera_.get()) : camera.get(),
		isEditorPreviewPlaying_, uiManager_.get(), enemies_);

	updateCoordinator_.UpdateSceneCamera({
		isDebugCameraActive_, canUseKeyboardInput, IsAutoFlightMode(),
		gameplayModeController_.GetTitleBackgroundFrame(),
		camera.get(), debugFlyCamera_.get(), player_.get(), &obstacles_,
		&cinematicCameraCoordinator_, &gameOverSequence_, &bossClearSequence_,
		&titleLaunchState_, &targetSelection_, lockOnManager_.get(),
		&warpExitPresentation_, cameraManager_.get(), &debugEditorController_,
		uiManager_.get(), simulationManager_.get(),
	});

	playerFlightEffectController_.UpdateAfterPlayerMovement({
		shouldUpdateGame, gameOverSequence_.IsActive(), IsTitleBackgroundMode(), gameplayElapsedFrames_,
		&gameplayBalanceSettings_.Get(), player_.get(), activeCamera, particleManager,
		&songState_, &songFlightTrailRenderer_, &warpExitPresentation_, &obstacles_,
	});

	for (Object3d *object3d : objects) {
		object3d->Update();
	}

	sprite->SetSize({ 300.0f, 300.0f });

	playerWeaponController_.UpdateWeaponInput({
		allowWeaponInput, gameOverSequence_.IsActive(), player_.get(), activeCamera,
		targetSelection_.GetLockedEnemy(), targetSelection_.GetAimAssistEnemy(),
		lockOnManager_.get(), missilePresetManager_.get(),
		&specialAttackState_, &multiLockChargeState_, &tutorialController_, &tutorialProgress_,
	});

	updateCoordinator_.UpdateMissilesAndHitEffects({
		updateSelectedMissiles, shouldUpdateGame, isBossClearSequenceActive, updateSelectedParticles,
		uiManager_ && uiManager_->IsDestructionEffectPreviewActive(),
		activeCamera, &enemies_, &obstacles_,
		missileManager_.get(), explosionManager_.get(), particleManager, &enemyDefeatEffectController_,
	});

	debugColliderRenderer_.Update({
		showDebugColliders, true, true,
		uiManager_ ? uiManager_->GetCurrentSimulationTarget() : 0,
		player_.get(), &obstacles_, &enemies_, targetSelection_.GetLockedEnemy(),
		missileManager_.get(), enemyBulletManager_.get(),
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
	GameplayWorldRenderer::Draw({
		renderCamera, isEditorPreviewPlaying_,
		warpExitPresentation_.IsActive() || introBoostState_.IsActive(),
		showPlane, showModel, showBones, showDebugColliders,
		tutorialController_.IsEnabled(), tutorialController_.GetPhaseFrames(),
		songState_.IsActive(), songState_.GetFrame(),
		environmentRenderer_.get(), player_.get(), missileManager_.get(), enemyBulletManager_.get(),
		&enemies_, &enemyDefeatEffectController_, &obstacles_, &objects, myModelObject.get(),
		&tutorialGoalMarker_, &boundaryShieldRenderer_, &boundaryShieldSettings_.Get(),
		skeletonLinesObject.get(), &debugColliderRenderer_, &warpExitPresentation_,
		&songFlightTrailRenderer_, explosionManager_.get(),
	});

	SpriteCommon::GetInstance()->SetCommonPipelineState();
	if (showSprite) {
		sprite->Draw();
	}
	DrawOverlay();
}

void GamePlayScene::UpdateTutorial() {
	const bool isBlocked = bossClearSequence_.IsActive() ||
		IsTitleBackgroundMode() || IsAttractDemoMode() || pauseMenuState_.IsPaused() ||
		gameOverSequence_.IsActive() || warpExitPresentation_.IsActive() || introBoostState_.IsActive();
	if (sessionFlowController_.UpdateTutorial({
			isBlocked, defeatedEnemyCount_, gameplayElapsedFrames_,
			player_.get(), &enemies_, &obstacles_, &enemySpawns_, &enemySpawnManager_,
			&bossEncounterState_, &targetSelection_, &tutorialController_, &tutorialProgress_,
			&tutorialRequirements_.Get(), &tutorialGoalMarker_,
			[this]() { ClearAllEnemyReferences(); },
		})) {
		SceneManager::GetInstance()->ChangeScene("CLEAR");
	}
}

void GamePlayScene::DrawOverlay() {
	if (isDebugCameraActive_ && !debugFlyCamera_) return;
	if (!isDebugCameraActive_ && !camera) return;

	Camera* activeCamera = isDebugCameraActive_ ? static_cast<Camera*>(debugFlyCamera_.get()) : camera.get();
	if (!activeCamera) return;

	overlayCoordinator_.Draw({
		activeCamera, IsTitleBackgroundMode(), IsAttractDemoMode(),
		warpFadeInAlpha_, gameplayBalanceSettings_.Get().songDurationFrames, gameplayElapsedFrames_,
		player_.get(), &enemies_, ammoManager_.get(), missileManager_.get(), lockOnManager_.get(),
		&warpExitPresentation_, &introBoostState_, &returnToTitleTransition_,
		&bossClearSequence_, &bossEncounterState_, &songState_, &specialAttackState_,
		&combatAlertState_, &targetSelection_, &multiLockChargeState_,
		&tutorialController_, &tutorialProgress_, &tutorialRequirements_.Get(), &tutorialGoalMarker_,
		&pauseMenuState_, &gameOverSequence_, &attractDemoController_.GetState(),
	});
}

void GamePlayScene::StartReturnToTitleTransition() {
	sessionFlowController_.StartReturnToTitleTransition(returnToTitleTransition_, audioController_);
}

void GamePlayScene::UpdateReturnToTitleTransition() {
	if (sessionFlowController_.UpdateReturnToTitleTransition(returnToTitleTransition_)) {
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}
