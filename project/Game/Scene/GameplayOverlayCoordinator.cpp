#include "GameplayOverlayCoordinator.h"

#include "2D/SpriteCommon.h"
#include "engine/Resource/TextureManager.h"
#include "AttractDemoState.h"
#include "BossClearSequenceController.h"
#include "BossEncounterState.h"
#include "GameOverSequence.h"
#include "IntroBoostState.h"
#include "LockOnManager.h"
#include "PauseMenuState.h"
#include "ReturnToTitleTransition.h"
#include "TargetSelectionState.h"
#include "TutorialPhaseController.h"
#include "TutorialRequirementsManager.h"
#include "WarpExitPresentationState.h"
#include "engine/base/WinApp.h"

#include <algorithm>

namespace {
	float SmoothStep01(float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}
}

void GameplayOverlayCoordinator::Initialize() {
	reticleOverlayRenderer_.Initialize();

	TextureManager::GetInstance()->LoadTexture("resources/white1x1.png");
	TextureManager::GetInstance()->LoadTexture("resources/mission_start_overlay.png");
	TextureManager::GetInstance()->LoadTexture("resources/mission_clear_overlay.png");
	TextureManager::GetInstance()->LoadTexture("resources/pause_menu_labels.png");

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

	pauseOverlaySprite_ = std::make_unique<Sprite>();
	pauseOverlaySprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");

	pausePanelSprite_ = std::make_unique<Sprite>();
	pausePanelSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");

	pauseMenuLabelsSprite_ = std::make_unique<Sprite>();
	pauseMenuLabelsSprite_->Initialize(SpriteCommon::GetInstance(), "resources/pause_menu_labels.png");

	combatHudSpriteRenderer_.Initialize();
	radarRenderer_.Initialize();
}

void GameplayOverlayCoordinator::Draw(const GameplayOverlayDrawContext& context) {
	if (!context.activeCamera || !context.enemies || !context.targetSelection) {
		return;
	}

	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());

	// 出撃カット中は開始演出だけを重ね、戦闘 HUD・レーダー・照準を一切出さない。
	if ((context.warpExitPresentation && context.warpExitPresentation->IsActive()) ||
		(context.introBoostState && context.introBoostState->IsActive())) {
		if (context.warpExitPresentation) {
			missionStartOverlayRenderer_.Draw(missionStartOverlaySprite_.get(), *context.warpExitPresentation);
		}
		DrawWarpFadeIn(context.warpFadeInAlpha, screenWidth, screenHeight);
		return;
	}

	if (context.isTitleBackgroundMode) {
		// タイトルでは戦闘 HUD を出さず、ロックオン表示だけを残す。
		reticleOverlayRenderer_.DrawTitleBackgroundLockOn(context.targetSelection->GetLockedEnemy(), context.activeCamera);
		return;
	}

	// タイトルへの帰還演出が始まった瞬間から、ゲームプレイ用の HUD・レーダー・
	// 照準・チュートリアルを一切描かない。終了演出だけを最前面に重ねることで、
	// フェード中に一部の UI が残る現象を防ぐ。
	if (context.returnToTitleTransition && context.returnToTitleTransition->IsActive()) {
		returnToTitleOverlayRenderer_.Draw(*context.returnToTitleTransition, pauseOverlaySprite_.get(),
			pausePanelSprite_.get(), screenWidth, screenHeight);
		return;
	}

	// 撃破演出中は通常 HUD を隠し、ボス本体と専用メッセージだけを見せる。
	if (context.bossClearSequence && context.bossClearSequence->IsActive()) {
		bossClearOverlayRenderer_.Draw(
			context.bossClearSequence->GetPhaseState(),
			context.bossClearSequence->GetTimeline(),
			context.bossClearSequence->GetEffectState(),
			BossClearSequenceController::kPhase2FlashDurationSec,
			BossClearSequenceController::kMissionClearDurationSec,
			bossCinematicTopBarSprite_.get(),
			bossCinematicBottomBarSprite_.get(),
			gameOverWhiteoutSprite_.get(),
			missionClearOverlaySprite_.get(),
			screenWidth,
			screenHeight);
		return;
	}

	const CombatHudLayout hudLayout = combatHudSpriteRenderer_.DrawBaseHud({
		screenWidth,
		screenHeight,
		context.songDurationFrames,
		context.player,
		context.enemies,
		context.ammoManager,
		context.songState,
		context.specialAttackState,
	});

	const bool isTutorialEnabled = context.tutorialController && context.tutorialController->IsEnabled();
	const bool isPaused = context.pauseMenuState && context.pauseMenuState->IsPaused();
	const bool isGameOverActive = context.gameOverSequence && context.gameOverSequence->IsActive();

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
		isTutorialEnabled,
		isPaused,
		isGameOverActive,
		context.gameplayElapsedFrames,
		context.player,
		context.activeCamera,
		context.ammoManager,
		context.songState,
		context.specialAttackState,
		context.combatAlertState,
	});

#ifdef CG2_ENABLE_GAME_UI
	const bool showStageObjective = !context.isTitleBackgroundMode && !context.isAttractDemoMode &&
		!isPaused && !isGameOverActive;
	stageObjectiveOverlayRenderer_.Draw(showStageObjective, context.player, *context.enemies,
		context.bossEncounterState && context.bossEncounterState->IsBossSpawned(), screenWidth, screenHeight);
#endif

	if (context.tutorialController && context.tutorialProgress && context.tutorialRequirements && context.tutorialGoalMarker) {
		tutorialOverlayRenderer_.Draw(*context.tutorialController, *context.tutorialProgress,
			*context.tutorialRequirements, *context.tutorialGoalMarker,
			context.player, context.activeCamera, context.enemies->size(), screenWidth, screenHeight);
	}

	if (context.multiLockChargeState) {
		radarRenderer_.Draw(context.player, *context.enemies, context.targetSelection->GetLockedEnemy(),
			*context.multiLockChargeState, context.missileManager);
	}

	const bool isJammed = context.lockOnManager && context.lockOnManager->IsPlayerJammed(context.activeCamera);
	reticleOverlayRenderer_.DrawLockAndAimCursor(
		context.player, context.targetSelection->GetLockedEnemy(), context.targetSelection->GetAimAssistEnemy(),
		context.lockOnManager, context.activeCamera, isJammed);
	combatHudSpriteRenderer_.DrawLockedTargetOverheadHp(
		context.targetSelection->GetLockedEnemy(), context.lockOnManager, context.activeCamera, screenWidth, screenHeight);
	if (context.multiLockChargeState) {
		reticleOverlayRenderer_.DrawMultiLockMarkers(
			*context.multiLockChargeState, context.missileManager, context.activeCamera, isJammed);
	}
	if (context.bossEncounterState) {
		bossIntroOverlayRenderer_.Draw(*context.bossEncounterState, screenWidth, screenHeight);
	}
	DrawPauseOverlay(context, screenWidth, screenHeight);

	// ワープ終了時のホワイトアウトからゲームステージへのフェードイン
	DrawWarpFadeIn(context.warpFadeInAlpha, screenWidth, screenHeight);

	// 機体の爆発中だけ中央に失敗通知を出し、リザルトへ移る前に確実に伝える。
	// ホワイトアウトの進行と同じ比率で消えるため、次の画面へ文字が残らない。
	if (context.gameOverSequence) {
		gameOverOverlayRenderer_.Draw(*context.gameOverSequence, screenWidth, screenHeight);
	}
	if (context.attractDemoState) {
		attractDemoOverlayRenderer_.Draw(
			context.isAttractDemoMode && !isGameOverActive,
			*context.attractDemoState, screenWidth, screenHeight);
	}

	if (isGameOverActive && gameOverWhiteoutSprite_) {
		const float whiteoutProgress = SmoothStep01(
			(static_cast<float>(context.gameOverSequence->GetFrame()) - 96.0f) /
			(static_cast<float>(GameOverSequence::kCinematicFrames) - 96.0f));
		gameOverWhiteoutSprite_->SetPosition({ 0.0f, 0.0f });
		gameOverWhiteoutSprite_->SetSize({ screenWidth, screenHeight });
		gameOverWhiteoutSprite_->SetColor({ 1.0f, 1.0f, 1.0f, whiteoutProgress });
		gameOverWhiteoutSprite_->Update();
		gameOverWhiteoutSprite_->Draw();
	}
}

void GameplayOverlayCoordinator::DrawPauseOverlay(const GameplayOverlayDrawContext& context,
	float screenWidth, float screenHeight) {
	if (context.returnToTitleTransition && context.returnToTitleTransition->IsActive()) {
		returnToTitleOverlayRenderer_.Draw(*context.returnToTitleTransition, pauseOverlaySprite_.get(),
			pausePanelSprite_.get(), screenWidth, screenHeight);
		return;
	}

	if (context.pauseMenuState) {
		pauseMenuOverlayRenderer_.Draw(*context.pauseMenuState, pauseOverlaySprite_.get(),
			pausePanelSprite_.get(), pauseMenuLabelsSprite_.get(), screenWidth, screenHeight);
	}
}

void GameplayOverlayCoordinator::DrawWarpFadeIn(float warpFadeInAlpha, float screenWidth, float screenHeight) {
	if (!warpFadeInSprite_ || warpFadeInAlpha <= 0.001f) {
		return;
	}
	warpFadeInSprite_->SetPosition({ 0.0f, 0.0f });
	warpFadeInSprite_->SetSize({ screenWidth, screenHeight });
	warpFadeInSprite_->SetColor({ 1.0f, 1.0f, 1.0f, warpFadeInAlpha });
	warpFadeInSprite_->Update();
	warpFadeInSprite_->Draw();
}
