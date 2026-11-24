#include "GameplayModeController.h"

#include "AttractDemoController.h"
#include "IntroBoostState.h"
#include "LockOnManager.h"
#include "TitleLaunchState.h"
#include "WarpExitPresentationState.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/math/MyMath.h"

#include <algorithm>
#include <cmath>

namespace {
Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 ScaleVector3(const Vector3& v, float scale) {
	return { v.x * scale, v.y * scale, v.z * scale };
}
} // namespace

GameplayInputPermissions GameplayModeController::EvaluateInputPermissions(
	GameplayRunMode mode,
	bool isImGuiKeyboardCapture,
	bool isImGuiMouseCapture) const {
	// タイトルメニューやアトラクトデモ中は本編側へ入力を渡さない。
	const bool isInteractiveMode = mode == GameplayRunMode::Gameplay;
	GameplayInputPermissions result{};
	result.canUseKeyboardInput = isInteractiveMode && !isImGuiKeyboardCapture;
	result.canUseMouseInput = isInteractiveMode && !isImGuiMouseCapture;
	result.canUsePlayerInput = result.canUseKeyboardInput && result.canUseMouseInput;
	return result;
}

bool GameplayModeController::ShouldClipMouseCursor(
	GameplayRunMode mode,
	bool isGameOverActive,
	bool isReturnToTitleActive,
	bool shouldUpdateGame,
	bool canUsePlayerInput,
	bool isImGuiVisible) const {
	// 通常プレイの視点操作中だけカーソルをゲーム画面内に固定する。
	return mode == GameplayRunMode::Gameplay &&
		!isGameOverActive &&
		!isReturnToTitleActive &&
		shouldUpdateGame &&
		canUsePlayerInput &&
		!isImGuiVisible;
}

GameplayFrameUpdatePolicy GameplayModeController::EvaluateFrameUpdatePolicy(const GameplayFramePolicyInput& input) const {
	GameplayFrameUpdatePolicy policy{};
	// 出撃カット中は敵の思考・弾・HUD を止める。入力や既存弾による被弾を
	// 演出に持ち込まないため、開始からアフターバーナー終了までを一続きに扱う。
	policy.isWarpExitSequenceActive = input.isWarpExitActive || input.isIntroBoostActive;
	// 移動キーは設定パネル上にマウスがあっても受け付ける。
	policy.updateSelectedPlayer = input.shouldUpdateGame && !input.isBossIntroSequenceActive && input.canUseKeyboardInput;
	policy.updateSelectedMissiles = input.shouldUpdateGame && !policy.isWarpExitSequenceActive && !input.isBossIntroSequenceActive;
	policy.updateSelectedEnemies = input.shouldUpdateGame && !policy.isWarpExitSequenceActive && !input.isBossIntroSequenceActive;
	policy.updateSelectedParticles = input.shouldUpdateGame;
	policy.allowWeaponInput = input.shouldUpdateGame && !policy.isWarpExitSequenceActive &&
		!input.isBossIntroSequenceActive && input.canUsePlayerInput;
	policy.allowLockOnBehavior = input.shouldUpdateGame && !policy.isWarpExitSequenceActive &&
		!input.isBossIntroSequenceActive && !input.isGameOverActive;
	policy.updateAnimationPreview = input.shouldUpdateGame;
	return policy;
}

void GameplayModeController::UpdateTitleLaunchPlayer(Player& player, TitleLaunchState& titleLaunchState) const {
	// START GAME 後もタイトルで見えていた地形を維持し、機体だけを一度ロールさせて出撃させる。
	const float progress = std::clamp(
		static_cast<float>(titleLaunchState.GetFrame()) / static_cast<float>(TitleLaunchState::kDurationFrames), 0.0f, 1.0f);
	// タイトル背景の巡航方向を変えずに加速する。カメラへ向かう補間や
	// ベジェ旋回を入れないため、画面下へ沈む不自然な軌道にならない。
	const float distance = progress * 8.0f + progress * progress * 30.0f;
	const Vector3 position = AddVector3(
		titleLaunchState.GetStartPosition(), ScaleVector3(titleLaunchState.GetForward(), distance));
	const float yaw = std::atan2(titleLaunchState.GetForward().x, titleLaunchState.GetForward().z);
	const float roll = progress * 6.28318531f;
	player.SetVelocity({
		titleLaunchState.GetForward().x * 0.78f,
		titleLaunchState.GetForward().y * 0.78f,
		titleLaunchState.GetForward().z * 0.78f,
	});
	player.UpdatePresentation(position, { -0.05f, yaw, roll }, 2.2f, true);

	titleLaunchState.AdvanceFrame();
}

void GameplayModeController::UpdateTitleBackgroundAutoFlight(Player& player, const std::list<std::unique_ptr<Obstacle>>& obstacles) {
	// StageBounds 内を「直線 + 壁際のUターン」で周回する。
	// 壁の手前で大きくバンクして旋回するため、タイトルを放置しても
	// フィールド外へ飛び出さず、直線飛行の見せ場も作れる。
	Vector3 stageCenter = { 0.0f, 0.0f, 0.0f };
	Vector3 stageHalfExtents = { 50.0f, 30.0f, 50.0f };
	if (const auto stageBoundsIt = std::find_if(obstacles.begin(), obstacles.end(),
		[](const std::unique_ptr<Obstacle>& obstacle) {
			return obstacle && obstacle->IsStageBounds();
		}); stageBoundsIt != obstacles.end()) {
		stageCenter = (*stageBoundsIt)->GetPosition();
		stageHalfExtents = (*stageBoundsIt)->GetWorldHalfExtents();
	}

	const Vector3 playerHalfExtents = player.GetWorldHalfExtents();
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
	player.SetVelocity(velocity);
	player.UpdatePresentation(position, { pitch, yaw, roll }, (std::max)(0.9f, horizontalSpeed * 7.0f), false);
}

void GameplayModeController::UpdatePlayerForMode(const GameplayPlayerModeContext& context) {
	if (!context.player || !context.obstacles) {
		return;
	}

	Player& player = *context.player;
	const auto& obstacles = *context.obstacles;

	if (context.mode == GameplayRunMode::TitleBackground &&
		context.titleLaunchState && context.titleLaunchState->IsActive()) {
		UpdateTitleLaunchPlayer(player, *context.titleLaunchState);
	} else if (context.mode == GameplayRunMode::TitleBackground) {
		UpdateTitleBackgroundAutoFlight(player, obstacles);
	} else if (context.mode == GameplayRunMode::AttractDemo) {
		if (context.attractDemoController) {
			context.attractDemoController->UpdatePilot(&player);
		}
	} else if (context.updateSelectedPlayer) {
		if (context.warpExitPresentation && context.warpExitPresentation->IsActive()) {
			context.warpExitPresentation->UpdatePlayer(player, context.particleManager);
		} else if (context.introBoostState && context.introBoostState->IsActive()) {
			const IntroBoostState::StepResult step = context.introBoostState->Advance(player.GetPosition());
			player.SetVelocity(step.velocity);
			player.UpdatePresentation(step.nextPosition, step.rotation, step.thrusterScale, true);
		} else {
			Vector3 lockOnTargetPosition;
			const Vector3* lockOnTarget = nullptr;
			if (context.lockedEnemy) {
				lockOnTargetPosition = (context.lockOnManager && context.lockOnManager->IsLockedEnemyAlive())
					? context.lockOnManager->GetLockedTargetPosition()
					: context.lockedEnemy->GetAimPosition();
				lockOnTarget = &lockOnTargetPosition;
			}
			player.Update(obstacles, lockOnTarget);
		}
	} else {
		// UI がキー入力を捕捉している間は、モデル更新だけを行い入力は渡さない。
		// 停止中もフリーカメラ用にWVPは更新するが、機体のアニメーションや補間は進めない。
		player.UpdateModel(context.canUseKeyboardInput, context.shouldUpdateGame);
	}

	// ゲーム／シミュレーション共通の編集停止中はプレイヤーの状態を動かさず、
	// スラスターだけを専用プレビューとして更新する。
	if (!context.isEditorPreviewPlaying && context.mode != GameplayRunMode::TitleBackground && player.GetBoosterEffect()) {
		player.GetBoosterEffect()->UpdateEditorPreview(player.GetPosition(), player.GetQuaternion(),
			static_cast<int>(player.GetCurrentMode()));
	}
}
