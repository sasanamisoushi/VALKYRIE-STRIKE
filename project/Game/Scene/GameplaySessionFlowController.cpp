#include "GameplaySessionFlowController.h"

#include "BossClearSequenceController.h"
#include "BossEncounterState.h"
#include "CombatAlertState.h"
#include "DamageFeedbackState.h"
#include "EnemyDefeatEffectController.h"
#include "EnemySpawnManager.h"
#include "Game/Player/Player.h"
#include "Game/Scene/ResultData.h"
#include "Game/Scene/TitleScene.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"
#include "GameOverSequence.h"
#include "GameplayAudioController.h"
#include "LockOnManager.h"
#include "PauseMenuState.h"
#include "ReturnToTitleTransition.h"
#include "TargetSelectionState.h"
#include "TutorialGoalMarker.h"
#include "TutorialPhaseController.h"
#include "TutorialProgressTracker.h"
#include "TutorialRequirementsManager.h"
#include "3D/Object3dCommon.h"
#include "Game/base/GameStartTransition.h"
#include "IntroBoostState.h"
#include "TitleBackgroundEnemyState.h"
#include "TitleLaunchState.h"
#include "WarpExitPresentationState.h"
#include "engine/Camera/Camera.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Input/Input.h"
#include "engine/Particle/ExplosionManager.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
	Vector3 AddVec3(const Vector3& a, const Vector3& b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vector3 SubtractVec3(const Vector3& a, const Vector3& b) {
		return { a.x - b.x, a.y - b.y, a.z - b.z };
	}

	Vector3 ScaleVec3(const Vector3& v, float s) {
		return { v.x * s, v.y * s, v.z * s };
	}

	Vector3 NormalizeOrVec3(const Vector3& value, const Vector3& fallback) {
		const float len = MyMath::Length(value);
		if (len <= 0.0001f) {
			return fallback;
		}
		return { value.x / len, value.y / len, value.z / len };
	}
}

void GameplaySessionFlowController::ApplyInitialStageTransition(const InitialStageTransitionContext& context) const {
	if (context.isAutoFlightMode || context.isAttractDemoMode || !context.player) {
		return;
	}
	GameStartTransitionData titleTransition;
	if (!GameStartTransition::Consume(titleTransition)) {
		return;
	}
	if (context.tutorialController) {
		context.tutorialController->Reset(titleTransition.stage == GameStartStage::Tutorial);
	}
	if (context.tutorialProgress) {
		context.tutorialProgress->Reset();
	}
	if (context.tutorialGoalMarker) {
		context.tutorialGoalMarker->Reset();
	}
	if (context.tutorialController && context.tutorialController->IsEnabled()) {
		if (context.clearAllEnemyReferences) {
			context.clearAllEnemyReferences();
		}
		if (context.enemies) {
			context.enemies->clear();
		}
		if (context.enemySpawns) {
			context.enemySpawns->clear();
		}
		if (context.enemySpawnManager) {
			context.enemySpawnManager->Clear();
		}
		if (context.bossEncounterState) {
			context.bossEncounterState->Reset();
		}
	}
	if (context.warpExitPresentation) {
		context.warpExitPresentation->Start(*context.player);
		if (context.introBoostState) {
			context.introBoostState->Reset(context.warpExitPresentation->GetYaw());
		}
	}
}

void GameplaySessionFlowController::ActivateFromWarp(const WarpActivationContext& context) const {
	if (context.titleLaunchState) context.titleLaunchState->Reset();
	if (context.titleBackgroundEnemyState) context.titleBackgroundEnemyState->Reset();

	if (context.camera) {
		Object3dCommon::GetInstance()->SetDefaultCamera(context.camera);
	}

	if (context.reloadSceneJson) {
		context.reloadSceneJson();
	}
	if (context.obstacles) {
		for (auto& obstacle : *context.obstacles) {
			if (obstacle) {
				obstacle->SetCollisionEnabled(true);
				obstacle->Update();
			}
		}
	}

	if (context.applyAudioSettings) {
		context.applyAudioSettings();
	}

	if (context.player) {
		context.player->SetSpecialAttackActive(false);
		context.player->ChangeMode(PlayerMode::Fighter);
		const Vector3 forward = context.player->GetForwardVector();
		const float baseSpeed = context.player->GetModeParams(PlayerMode::Fighter).maxMoveSpeed * 0.8f;
		context.player->SetVelocity({ forward.x * baseSpeed, forward.y * baseSpeed, forward.z * baseSpeed });
	}

	GameStartTransitionData titleTransition;
	GameStartTransition::Consume(titleTransition);
	if (context.introBoostState) context.introBoostState->Reset();
	if (context.gameOverSequence) context.gameOverSequence->Reset();
	if (context.combatAlertState) context.combatAlertState->Reset();
	if (context.damageFeedbackState) context.damageFeedbackState->Reset();
	if (context.bossClearSequence) context.bossClearSequence->Reset();
	if (context.defeatedEnemyCount) *context.defeatedEnemyCount = 0;
	if (context.gameplayElapsedFrames) *context.gameplayElapsedFrames = 0;
	if (context.pauseMenuState) context.pauseMenuState->Reset();
	if (context.isEditorPreviewPlaying) *context.isEditorPreviewPlaying = true;
	if (context.warpFadeInAlpha) *context.warpFadeInAlpha = 1.0f;
	if (context.tutorialController) context.tutorialController->Reset(context.tutorialController->IsEnabled());
	if (context.tutorialProgress) context.tutorialProgress->Reset();
	if (context.tutorialGoalMarker) context.tutorialGoalMarker->Reset();
}

void GameplaySessionFlowController::ResetEditorPreview(const EditorPreviewResetContext& context) const {
	// チュートリアルの敵は scene.json ではなく BeginTutorialCombat() が専用に生成する。
	// 訓練中は現在の配置を保ったまま編集停止へ移行する。
	const bool preserveTutorialLayout = context.tutorialController &&
		context.tutorialController->IsEnabled() &&
		context.tutorialController->GetPhase() != TutorialPhase::Disabled;
	Vector3 tutorialPlayerPosition{};
	Vector3 tutorialPlayerForward{};
	PlayerMode tutorialPlayerMode = PlayerMode::Fighter;
	if (preserveTutorialLayout && context.player) {
		tutorialPlayerPosition = context.player->GetPosition();
		tutorialPlayerForward = context.player->GetForwardVector();
		tutorialPlayerMode = context.player->GetCurrentMode();
	}

	if (context.isEditorPreviewPlaying) {
		*context.isEditorPreviewPlaying = false;
	}
	if (context.gameOverSequence) {
		context.gameOverSequence->Reset();
	}
	if (context.combatAlertState) {
		context.combatAlertState->Reset();
	}
	if (context.damageFeedbackState) {
		context.damageFeedbackState->Reset();
	}
	if (context.bossClearSequence) {
		context.bossClearSequence->Reset();
	}
	if (context.enemyDefeatEffectController) {
		context.enemyDefeatEffectController->Clear();
	}
	if (context.clearAllEnemyReferences) {
		context.clearAllEnemyReferences();
	}
	if (context.lockOnManager) {
		context.lockOnManager->CancelMultiLock();
	}

	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetEffectType(0);
	}

	if (context.player && context.playerModelName) {
		context.player->Initialize(context.playerModelName);
	}
	if (context.missileManager) {
		context.missileManager->Initialize(context.particleManager);
	}
	if (context.enemyBulletManager) {
		context.enemyBulletManager->Initialize();
	}
	if (context.explosionManager) {
		context.explosionManager->Initialize(context.particleManager);
	}

	if (preserveTutorialLayout) {
		// Initialize() によるプレイヤーの初期位置への巻き戻しも防ぐ。
		if (context.player) {
			context.player->SetPosition(tutorialPlayerPosition);
			context.player->ChangeMode(tutorialPlayerMode);
			const float horizontalLength = std::sqrt(
				tutorialPlayerForward.x * tutorialPlayerForward.x +
				tutorialPlayerForward.z * tutorialPlayerForward.z);
			if (horizontalLength > 0.0001f) {
				const float yaw = std::atan2(tutorialPlayerForward.x, tutorialPlayerForward.z);
				context.player->SetRotation({ 0.0f, yaw, 0.0f });
			}
		}
	} else if (context.reloadSceneJson) {
		context.reloadSceneJson();
	}

	if (!context.isDebugCameraActive && context.player) {
		Vector3* targetPos = nullptr;
		Vector3 enemyPos{};
		if (context.targetSelection) {
			if (Enemy* lockedEnemy = context.targetSelection->GetLockedEnemy()) {
				enemyPos = lockedEnemy->GetPosition();
				targetPos = &enemyPos;
			}
		}
		context.player->UpdateCamera(context.camera, targetPos);
	}

	if (context.player) {
		context.player->UpdateModel(false, false);
		if (context.player->GetBoosterEffect()) {
			context.player->GetBoosterEffect()->UpdateEditorPreview(
				context.player->GetPosition(), context.player->GetQuaternion(),
				static_cast<int>(context.player->GetCurrentMode()));
		}
	}
	OutputDebugStringA("[EditorPreview] Reset scene and paused.\n");
}

void GameplaySessionFlowController::CancelPauseForCinematic(
	PauseMenuState& pauseMenuState, bool isSongActive, GameplayAudioController& audioController) const {
	if (!pauseMenuState.IsPaused()) {
		return;
	}
	pauseMenuState.Resume();
	// ポーズ開始時に止めた歌も、演出時間の進行と同じ状態へ戻す。
	audioController.SetSongPaused(isSongActive, false);
}

bool GameplaySessionFlowController::UpdatePauseMenu(const PauseMenuUpdateContext& context) const {
	if (!context.pauseMenuState || !context.audioController) {
		return false;
	}
	if (context.isPauseBlockedByCinematic) {
		CancelPauseForCinematic(*context.pauseMenuState, context.isSongActive, *context.audioController);
		return false;
	}
	if (!context.canUseKeyboardInput) {
		return false;
	}

	Input* input = Input::GetInstance();
	const bool togglePause = input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_START);
	const bool cancelPause = context.pauseMenuState->IsPaused() && input->TriggerControllerButton(XINPUT_GAMEPAD_B);

	if (togglePause || cancelPause) {
		const bool isNowPaused = context.pauseMenuState->Toggle();
		// 歌のループ音もゲーム進行と同じタイミングで止める。
		context.audioController->SetSongPaused(context.isSongActive, isNowPaused);
		return false;
	}

	if (!context.pauseMenuState->IsPaused()) {
		return false;
	}

	const bool navMove = input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_DOWN) ||
		input->TriggerKey(DIK_W) || input->TriggerKey(DIK_S) ||
		input->TriggerControllerInput(ControllerInput::DPadUp) || input->TriggerControllerInput(ControllerInput::DPadDown) ||
		input->TriggerControllerInput(ControllerInput::LeftStickUp) || input->TriggerControllerInput(ControllerInput::LeftStickDown);
	if (navMove) {
		context.pauseMenuState->ToggleSelection();
	}

	const bool confirm = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
		input->TriggerControllerButton(XINPUT_GAMEPAD_A);
	if (!confirm) {
		return false;
	}

	if (context.pauseMenuState->IsTitleSelected() && context.returnToTitleTransition) {
		StartReturnToTitleTransition(*context.returnToTitleTransition, *context.audioController);
		return true;
	}

	context.pauseMenuState->Resume();
	context.audioController->SetSongPaused(context.isSongActive, false);
	return false;
}

void GameplaySessionFlowController::StartReturnToTitleTransition(
	ReturnToTitleTransition& transition, GameplayAudioController& audioController) const {
	if (!transition.Start()) {
		return;
	}

	// 通信切断: BGMやボイスの音量を即座に落として通信途絶感を出す
	audioController.MuteAllForShutdown();

	// ポストエフェクトを即座に通信切断・シャットダウン初期状態（ノイズ・走査線）に切り替え
	if (PostEffect::GetInstance()) {
		PostEffect::GetInstance()->SetSystemShutdown(0.0f, 1.0f);
	}
}

bool GameplaySessionFlowController::UpdateReturnToTitleTransition(ReturnToTitleTransition& transition) const {
	const bool transitionFinished = transition.Advance();
	const float progress = transition.GetProgress();

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
		return true;
	}
	return false;
}

void GameplaySessionFlowController::BeginTutorialCombat(const TutorialSessionContext& context) const {
	if (!context.tutorialController || context.tutorialController->HasCombatTargets() ||
		!context.player || !context.enemies) {
		return;
	}

	// 本編のスポーン情報をチュートリアルへ持ち込まない。ここで生成する3機だけが
	// 攻撃フェーズの対象になり、プレイヤーを追尾・攻撃しない。
	if (context.clearAllEnemyReferences) {
		context.clearAllEnemyReferences();
	}
	context.enemies->clear();
	if (context.enemySpawns) {
		context.enemySpawns->clear();
	}
	if (context.enemySpawnManager) {
		context.enemySpawnManager->Clear();
	}
	if (context.bossEncounterState) {
		context.bossEncounterState->Reset();
	}

	const Vector3 playerPosition = context.player->GetPosition();
	const Vector3 forward = NormalizeOrVec3(context.player->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	const Vector3 right = NormalizeOrVec3({ forward.z, 0.0f, -forward.x }, { 1.0f, 0.0f, 0.0f });
	const std::array<Vector3, 3> targetOffsets = {
		AddVec3(ScaleVec3(forward, 22.0f), { 0.0f, 1.5f, 0.0f }),
		AddVec3(AddVec3(ScaleVec3(forward, 27.0f), ScaleVec3(right, -7.0f)), { 0.0f, 4.5f, 0.0f }),
		AddVec3(AddVec3(ScaleVec3(forward, 31.0f), ScaleVec3(right, 7.0f)), { 0.0f, -1.5f, 0.0f }),
	};

	for (const Vector3& offset : targetOffsets) {
		auto target = std::make_unique<Enemy>();
		target->Initialize(AddVec3(playerPosition, offset));
		target->SetTrainingTarget(true);
		context.enemies->push_back(std::move(target));
	}
	context.tutorialController->MarkCombatTargetsSpawned();
}

bool GameplaySessionFlowController::UpdateTutorial(const TutorialSessionContext& context) const {
	if (context.isBlocked || !context.tutorialController || !context.tutorialProgress ||
		!context.tutorialRequirements || !context.tutorialGoalMarker || !context.enemies ||
		!context.tutorialController->IsEnabled() ||
		context.tutorialController->GetPhase() == TutorialPhase::Disabled) {
		return false;
	}

	Input* input = Input::GetInstance();
	if (!input) {
		return false;
	}

	TutorialPhaseInput tutorialInput;
	tutorialInput.skipRequested = input->TriggerKey(DIK_F8);
	tutorialInput.forwardTriggered = input->TriggerAction(PlayerAction::MoveForward);
	// マウスだけでなく、コントローラーのピッチ操作でも飛行訓練を達成できるようにする。
	tutorialInput.altitudeInput = input->GetMouseDeltaY() != 0 ||
		input->PushAction(PlayerAction::PitchUp) || input->PushAction(PlayerAction::PitchDown);
	tutorialInput.turnTriggered = input->TriggerAction(PlayerAction::DodgeLeft) || input->TriggerAction(PlayerAction::DodgeRight);
	tutorialInput.isGerwalk = context.player && context.player->GetCurrentMode() == PlayerMode::Gerwalk;
	tutorialInput.gerwalkHoverTriggered = input->TriggerAction(PlayerAction::MoveUp) || input->TriggerAction(PlayerAction::MoveDown);
	tutorialInput.gerwalkStrafeTriggered = tutorialInput.turnTriggered;
	tutorialInput.isLocked = context.targetSelection && context.targetSelection->HasAliveLockedEnemy();
	tutorialInput.isFighter = context.player && context.player->GetCurrentMode() == PlayerMode::Fighter;
	tutorialInput.combatTargetsDefeated = context.enemies->empty();
	tutorialInput.goalReached = context.tutorialGoalMarker->IsActive() && context.player &&
		MyMath::Length(SubtractVec3(context.player->GetPosition(), context.tutorialGoalMarker->GetPosition())) <= 3.5f;
	tutorialInput.completeConfirmed = input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) ||
		input->TriggerControllerButton(XINPUT_GAMEPAD_A) || input->TriggerControllerButton(XINPUT_GAMEPAD_START);

	const TutorialPhaseResult result = context.tutorialController->Update(
		tutorialInput, *context.tutorialRequirements, *context.tutorialProgress);
	if (result.clearGoal) {
		context.tutorialGoalMarker->Deactivate();
	}
	if (result.beginCombat) {
		BeginTutorialCombat(context);
	}
	if (result.activateGoal && context.obstacles) {
		context.tutorialGoalMarker->Activate(context.player, *context.obstacles);
	}
	if (result.clearMission && context.player) {
		MissionResultStore::Set({
			MissionResultOutcome::Clear,
			context.defeatedEnemyCount,
			context.player->GetHP(),
			context.gameplayElapsedFrames,
			false,
			MissionStage::Tutorial,
		});
		return true;
	}
	return false;
}
