#include "TutorialOverlayRenderer.h"

#include "TutorialGoalMarker.h"
#include "TutorialPhaseController.h"
#include "TutorialProgressTracker.h"
#include "TutorialRequirementsManager.h"
#include "Game/Player/Player.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Debug/ImGuiManager.h"
#include "engine/Input/Input.h"
#include "engine/math/MyMath.h"

#ifdef CG2_ENABLE_GAME_UI
#include <externals/imgui/imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <string>

namespace {
Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

Vector3 NormalizeOrVector3(const Vector3& value, const Vector3& fallback) {
	const float length = MyMath::Length(value);
	if (length <= 0.0001f) {
		return fallback;
	}
	return { value.x / length, value.y / length, value.z / length };
}
}

void TutorialOverlayRenderer::Draw(const TutorialPhaseController& controller, const TutorialProgressTracker& progress,
	const TutorialRequirements& requirements, const TutorialGoalMarker& goalMarker,
	const Player* player, Camera* navigationCamera, std::size_t remainingEnemyCount,
	float screenWidth, float screenHeight) const {
#ifdef CG2_ENABLE_GAME_UI
	if (controller.GetPhase() == TutorialPhase::Disabled || !ImGui::GetCurrentContext()) {
		return;
	}

	float gameViewMinX = 0.0f;
	float gameViewMinY = 0.0f;
	float gameViewMaxX = screenWidth;
	float gameViewMaxY = screenHeight;
	if (ImGuiManager::IsVisible() &&
		!FlyCamera::GetGameViewBounds(gameViewMinX, gameViewMinY, gameViewMaxX, gameViewMaxY)) {
		return;
	}
	const float viewScaleX = (gameViewMaxX - gameViewMinX) / (std::max)(screenWidth, 1.0f);
	const float viewScaleY = (gameViewMaxY - gameViewMinY) / (std::max)(screenHeight, 1.0f);
	auto toGameView = [=](float x, float y) {
		return ImVec2(gameViewMinX + x * viewScaleX, gameViewMinY + y * viewScaleY);
	};
	const float scale = (std::min)(viewScaleX, viewScaleY);
	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());

	const char* title = "";
	const char* message = "";
	const char* supportText = "";
	std::string progressText;
	std::string inputHint;
	const auto actionProgress = [](int count, int requiredCount) {
		requiredCount = (std::max)(requiredCount, 1);
		return std::to_string((std::min)(count, requiredCount) * 100 / requiredCount) + "%";
	};
	const bool useControllerGuide = Input::GetInstance()->IsControllerConnected();
	const auto inputForAction = [useControllerGuide](PlayerAction action) {
		const PlayerActionBinding& binding = Input::GetInstance()->GetActionBinding(action);
		if (useControllerGuide && binding.controllerInput != ControllerInput::None) {
			return std::string(Input::GetControllerInputName(binding.controllerInput));
		}
		if (binding.keyboardKey != 0) {
			return Input::GetKeyboardKeyName(binding.keyboardKey);
		}
		if (binding.mouseInput != MouseInput::None) {
			return std::string(Input::GetMouseInputName(binding.mouseInput));
		}
		if (binding.controllerInput != ControllerInput::None) {
			return std::string(Input::GetControllerInputName(binding.controllerInput));
		}
		return std::string("未設定");
	};
	int step = 0;
	float highlightX = 0.0f;
	float highlightY = 0.0f;
	float highlightWidth = 0.0f;
	float highlightHeight = 0.0f;
	const float guideHeight = player && player->GetCurrentMode() == PlayerMode::Gerwalk ? 278.0f : 256.0f;

	switch (controller.GetPhase()) {
	case TutorialPhase::Flight:
		step = 1;
		title = "ファイター飛行訓練";
		message = "加速・マウス上下・A/D旋回を、それぞれ規定回数行ってください。";
		supportText = "FIGHTER: high-speed flight and evasive movement";
		progressText = "前進 " + actionProgress(progress.GetForwardCount(), requirements.forward) + "  高度 " +
			actionProgress(progress.GetAltitudeCount(), requirements.altitude) + "  旋回 " +
			actionProgress(progress.GetTurnCount(), requirements.turn);
		// 接続中の入力機器に合わせ、キーコンフィグ後も実際の割り当てを案内する。
		inputHint = "前進: " + inputForAction(PlayerAction::MoveForward) +
			"  /  高度: " + (useControllerGuide
				? inputForAction(PlayerAction::PitchUp) + "・" + inputForAction(PlayerAction::PitchDown)
				: std::string("マウス上下")) +
			"  /  旋回: " + inputForAction(PlayerAction::DodgeLeft) + "・" + inputForAction(PlayerAction::DodgeRight);
		highlightX = 18.0f; highlightY = 18.0f; highlightWidth = 412.0f; highlightHeight = guideHeight + 12.0f;
		break;
	case TutorialPhase::Gerwalk:
		step = 2;
		title = "ガウォーク機動訓練";
		message = "ガウォークを維持し、上下移動と横移動をそれぞれ規定回数行ってください。";
		supportText = "GERWALK: hover, strafe, and keep your aim steady";
		progressText = "変形 " + std::to_string(player && player->GetCurrentMode() == PlayerMode::Gerwalk) +
			"/1  滞空 " + actionProgress(progress.GetGerwalkHoverCount(), requirements.gerwalkHover) + "  横移動 " +
			actionProgress(progress.GetGerwalkStrafeCount(), requirements.gerwalkStrafe);
		inputHint = "ガウォーク: " + inputForAction(PlayerAction::TransformGerwalk) +
			"  /  滞空: " + inputForAction(PlayerAction::MoveUp) + "・" + inputForAction(PlayerAction::MoveDown) +
			"  /  横移動: " + inputForAction(PlayerAction::DodgeLeft) + "・" + inputForAction(PlayerAction::DodgeRight);
		highlightX = 18.0f; highlightY = 18.0f; highlightWidth = 412.0f; highlightHeight = guideHeight + 12.0f;
		break;
	case TutorialPhase::LockOn:
		step = 3;
		title = "攻撃フェーズ：ロックオン";
		message = "照準内の訓練標的を、規定回数ロックしてください。";
		supportText = "Press lock once to select a target. Press release to cancel it.";
		progressText = "ロック  " + actionProgress(progress.GetLockCount(), requirements.lockOn);
		inputHint = "ロック: " + inputForAction(PlayerAction::LockToggle) +
			"  /  解除: " + inputForAction(PlayerAction::LockRelease);
		break;
	case TutorialPhase::NormalFire:
		step = 3;
		title = "攻撃フェーズ：主砲";
		message = "主砲を出現した訓練標的へ、規定回数発射してください。";
		supportText = "Three stationary targets are deployed for weapons practice";
		progressText = "主砲発射 " + actionProgress(progress.GetNormalShotsFired(), requirements.normalShots);
		inputHint = "主砲発射: " + inputForAction(PlayerAction::NormalFire);
		highlightX = screenWidth - 445.0f; highlightY = screenHeight - 355.0f; highlightWidth = 430.0f; highlightHeight = 160.0f;
		break;
	case TutorialPhase::MultiLock:
		step = 3;
		title = "攻撃フェーズ：複数ロック";
		message = "ファイターへ変形し、標的へ規定回数ミサイルを発射します。";
		supportText = "FIGHTER: long-range multi-lock and homing missiles";
		progressText = "ファイター " + actionProgress(progress.GetFighterTransformCount(), requirements.fighterTransform) + "  ミサイル " +
			actionProgress(progress.GetHomingLaunches(), requirements.homingLaunches);
		inputHint = "ファイター: " + inputForAction(PlayerAction::TransformFighter) +
			"  /  ホーミング: " + inputForAction(PlayerAction::HomingFire) + " を長押し→離す";
		highlightX = screenWidth - 445.0f; highlightY = screenHeight - 190.0f; highlightWidth = 430.0f; highlightHeight = 160.0f;
		break;
	case TutorialPhase::Combat:
		step = 3;
		title = "攻撃フェーズ：標的を殲滅";
		message = "残った訓練標的をすべて撃破してください。";
		supportText = "Destroy every training target to unlock the simulation exit";
		progressText = "残存標的  " + std::to_string(remainingEnemyCount) + " / 3";
		inputHint = "主砲・ホーミングミサイルで標的を撃破";
		break;
	case TutorialPhase::Navigation:
		step = 3;
		title = "チュートリアル完了まであと少し！";
		message = "矢印をたどり、マップ中央の青いリングの中へ飛び込んでください。";
		supportText = "Follow the GOAL arrow and fly through the blue ring at the map centre";
		if (goalMarker.IsActive() && player) {
			const int distance = static_cast<int>(MyMath::Length(SubtractVector3(goalMarker.GetPosition(), player->GetPosition())));
			progressText = "中央の青いリングまで  " + std::to_string(distance) + " m";
		} else {
			progressText = "目標地点  到達";
		}
		inputHint = "GOAL矢印の方向へ飛行  →  青いリングを通過";
		break;
	case TutorialPhase::Complete:
		step = 3;
		title = "シミュレーション完了";
		message = "すべての訓練を達成しました。";
		supportText = "All training objectives complete";
		progressText = "システムオンライン";
		inputHint = useControllerGuide ? "A: 続行" : "ENTER: 続行";
		break;
	case TutorialPhase::Disabled:
		return;
	}

	const float pulse = 0.72f + 0.28f * std::sin(static_cast<float>(controller.GetPhaseFrames()) * 0.12f);
	const ImU32 cyan = IM_COL32(76, 232, 255, static_cast<int>(255.0f * pulse));
	const ImU32 softCyan = IM_COL32(76, 232, 255, 74);
	if (highlightWidth > 0.0f && highlightHeight > 0.0f) {
		drawList->AddRectFilled(toGameView(highlightX - 5.0f, highlightY - 5.0f),
			toGameView(highlightX + highlightWidth + 5.0f, highlightY + highlightHeight + 5.0f), softCyan, 7.0f);
		drawList->AddRect(toGameView(highlightX, highlightY),
			toGameView(highlightX + highlightWidth, highlightY + highlightHeight), cyan, 6.0f, 2.0f * scale, 0);
	}

	const float panelWidth = (std::min)(620.0f, (std::max)(320.0f, screenWidth - 80.0f));
	const float panelHeight = 172.0f;
	const float panelX = (screenWidth - panelWidth) * 0.5f;
	// 視線を下へ大きく逸らさず読める、画面中央より少し上の位置へ固定する。
	const float panelY = (std::max)(112.0f, screenHeight * 0.22f - panelHeight * 0.5f);
	drawList->AddRectFilled(toGameView(panelX, panelY), toGameView(panelX + panelWidth, panelY + panelHeight),
		IM_COL32(3, 14, 31, 232), 7.0f);
	drawList->AddRect(toGameView(panelX, panelY), toGameView(panelX + panelWidth, panelY + panelHeight),
		IM_COL32(65, 185, 220, 230), 7.0f, 1.5f * scale, 0);
	drawList->AddRectFilled(toGameView(panelX, panelY), toGameView(panelX + panelWidth, panelY + 29.0f),
		IM_COL32(8, 56, 84, 238), 7.0f);
	drawList->AddLine(toGameView(panelX + 14.0f, panelY + 30.0f), toGameView(panelX + panelWidth - 14.0f, panelY + 30.0f),
		cyan, 1.0f * scale);

	const std::string header = "操縦支援 // フェーズ " + std::to_string(step) + "/3";
	drawList->AddText(nullptr, 15.0f * scale, toGameView(panelX + 16.0f, panelY + 7.0f),
		IM_COL32(176, 236, 255, 255), header.c_str());
	drawList->AddText(nullptr, 22.0f * scale, toGameView(panelX + 16.0f, panelY + 42.0f), cyan, title);
	drawList->AddText(nullptr, 16.0f * scale, toGameView(panelX + 16.0f, panelY + 70.0f),
		IM_COL32(221, 240, 248, 255), message);
	drawList->AddText(nullptr, 12.0f * scale, toGameView(panelX + 16.0f, panelY + 93.0f),
		IM_COL32(153, 207, 224, 255), supportText);
	drawList->AddText(nullptr, 13.0f * scale, toGameView(panelX + 16.0f, panelY + 111.0f),
		IM_COL32(124, 238, 255, 255), progressText.c_str());
	drawList->AddRectFilled(toGameView(panelX + 16.0f, panelY + 133.0f), toGameView(panelX + panelWidth - 16.0f, panelY + 160.0f),
		IM_COL32(12, 74, 104, 230), 3.0f);
	drawList->AddText(nullptr, 14.0f * scale, toGameView(panelX + 27.0f, panelY + 138.0f),
		IM_COL32(255, 226, 112, 255), inputHint.c_str());
	drawList->AddText(nullptr, 12.0f * scale, toGameView(panelX + panelWidth - 97.0f, panelY + 9.0f),
		IM_COL32(130, 178, 195, 225), "F8: SKIP");

	// 最終目標は、リングが画面外にあっても方向を失わないように常時ナビゲーション
	// 矢印を出す。画面内ではリングの上、画面外では画面端に GOAL を固定する。
	if (controller.GetPhase() == TutorialPhase::Navigation && goalMarker.IsActive() && player) {
		const Vector3 toGoal = SubtractVector3(goalMarker.GetPosition(), player->GetPosition());
		const Vector3 forward = NormalizeOrVector3(player->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
		const Vector3 right = NormalizeOrVector3({ forward.z, 0.0f, -forward.x }, { 1.0f, 0.0f, 0.0f });
		float directionX = MyMath::Dot(toGoal, right);
		float directionY = -MyMath::Dot(toGoal, forward);
		const float directionLength = std::sqrt(directionX * directionX + directionY * directionY);
		if (directionLength > 0.001f) {
			directionX /= directionLength;
			directionY /= directionLength;
		} else {
			directionX = 0.0f;
			directionY = -1.0f;
		}

		float indicatorX = screenWidth * 0.5f + directionX * screenWidth * 0.38f;
		float indicatorY = screenHeight * 0.5f + directionY * screenHeight * 0.32f;
		bool goalIsOnScreen = false;
		if (navigationCamera) {
			const Vector3 goalScreen = MyMath::WorldToScreen(
				goalMarker.GetPosition(), navigationCamera->GetViewProjectionMatrix(), screenWidth, screenHeight);
			goalIsOnScreen = goalScreen.z >= 0.0f && goalScreen.z <= 1.0f &&
				goalScreen.x >= 42.0f && goalScreen.x <= screenWidth - 42.0f &&
				goalScreen.y >= 72.0f && goalScreen.y <= screenHeight - 42.0f;
			if (goalIsOnScreen) {
				indicatorX = goalScreen.x;
				indicatorY = goalScreen.y - 54.0f;
				directionX = 0.0f;
				directionY = 1.0f;
			}
		}

		indicatorX = std::clamp(indicatorX, 48.0f, screenWidth - 48.0f);
		indicatorY = std::clamp(indicatorY, 64.0f, screenHeight - 48.0f);
		const float arrowPulse = 0.82f + 0.18f * std::sin(static_cast<float>(controller.GetPhaseFrames()) * 0.18f);
		const float arrowSize = 25.0f * arrowPulse;
		const float perpendicularX = -directionY;
		const float perpendicularY = directionX;
		const ImVec2 tip = toGameView(indicatorX + directionX * arrowSize, indicatorY + directionY * arrowSize);
		const ImVec2 left = toGameView(indicatorX - directionX * arrowSize * 0.55f + perpendicularX * arrowSize * 0.62f,
			indicatorY - directionY * arrowSize * 0.55f + perpendicularY * arrowSize * 0.62f);
		const ImVec2 rightPoint = toGameView(indicatorX - directionX * arrowSize * 0.55f - perpendicularX * arrowSize * 0.62f,
			indicatorY - directionY * arrowSize * 0.55f - perpendicularY * arrowSize * 0.62f);
		const ImU32 goalColor = IM_COL32(74, 238, 255, static_cast<int>(255.0f * arrowPulse));
		drawList->AddCircleFilled(toGameView(indicatorX, indicatorY), 34.0f * scale, IM_COL32(3, 40, 68, 205), 28);
		drawList->AddCircle(toGameView(indicatorX, indicatorY), 34.0f * scale, goalColor, 28, 2.5f * scale);
		drawList->AddTriangleFilled(tip, left, rightPoint, goalColor);
		const char* goalLabel = goalIsOnScreen ? "GOAL  ↓" : "GOAL";
		const ImVec2 labelSize = ImGui::CalcTextSize(goalLabel);
		drawList->AddText(nullptr, 15.0f * scale,
			toGameView(indicatorX - labelSize.x * 0.5f, indicatorY - 58.0f),
			IM_COL32(220, 253, 255, 255), goalLabel);
	}
#else
	(void)screenWidth;
	(void)screenHeight;
	(void)controller;
	(void)progress;
	(void)requirements;
	(void)goalMarker;
	(void)player;
	(void)navigationCamera;
	(void)remainingEnemyCount;
#endif
}
