#include "BossIntroOverlayRenderer.h"

#include "BossEncounterState.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Debug/ImGuiManager.h"

#ifdef CG2_ENABLE_GAME_UI
#include <externals/imgui/imgui.h>

#include <algorithm>
#include <cmath>

namespace {
	float SmoothStep01(float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}
}
#endif

void BossIntroOverlayRenderer::Draw(const BossEncounterState& state, float screenWidth, float screenHeight) const {
#ifdef CG2_ENABLE_GAME_UI
	if (!state.IsIntroActive() || !ImGui::GetCurrentContext()) {
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
	const float elapsed = static_cast<float>(BossEncounterState::kIntroDurationFrames - state.GetIntroFrames());
	const float progress = std::clamp(elapsed / static_cast<float>(BossEncounterState::kIntroDurationFrames), 0.0f, 1.0f);
	const float alpha = (std::min)(SmoothStep01(progress / 0.12f), 1.0f - SmoothStep01((progress - 0.86f) / 0.14f));
	if (alpha <= 0.001f) {
		return;
	}

	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
	drawList->AddRectFilled(toGameView(0.0f, 0.0f), toGameView(screenWidth, screenHeight), IM_COL32(8, 0, 4, static_cast<int>(135.0f * alpha)));
	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.48f;
	const float pulse = 0.5f + 0.5f * std::sin(elapsed * 0.32f);
	const ImU32 red = IM_COL32(255, 48, 70, static_cast<int>(255.0f * alpha));
	const ImU32 softRed = IM_COL32(255, 32, 54, static_cast<int>((85.0f + 70.0f * pulse) * alpha));
	for (int index = 0; index < 3; ++index) {
		const float phase = std::fmod(progress * 1.45f + static_cast<float>(index) / 3.0f, 1.0f);
		const float radius = (96.0f + phase * 310.0f) * scale;
		const int ringAlpha = static_cast<int>((1.0f - phase) * 190.0f * alpha);
		drawList->AddCircle(toGameView(centerX, centerY), radius, IM_COL32(255, 34, 55, ringAlpha), 72, 3.0f * scale);
	}
	drawList->AddCircleFilled(toGameView(centerX, centerY), (82.0f + pulse * 10.0f) * scale, IM_COL32(92, 0, 12, static_cast<int>(138.0f * alpha)), 64);
	drawList->AddCircle(toGameView(centerX, centerY), (98.0f + pulse * 8.0f) * scale, red, 64, 4.0f * scale);
	drawList->AddCircle(toGameView(centerX, centerY), 72.0f * scale, softRed, 64, 2.0f * scale);

	const char* warning = "WARNING";
	const ImVec2 warningSize = ImGui::CalcTextSize(warning);
	const float warningX = centerX - warningSize.x * 0.5f;
	drawList->AddText(nullptr, 52.0f * scale, toGameView(warningX + 3.0f, centerY - 34.0f + 4.0f), IM_COL32(16, 0, 3, static_cast<int>(245.0f * alpha)), warning);
	drawList->AddText(nullptr, 52.0f * scale, toGameView(warningX, centerY - 34.0f), red, warning);
	const char* incoming = "BOSS INCOMING";
	const ImVec2 incomingSize = ImGui::CalcTextSize(incoming);
	drawList->AddText(nullptr, 18.0f * scale, toGameView(centerX - incomingSize.x * 0.5f, centerY + 34.0f), IM_COL32(255, 220, 220, static_cast<int>(255.0f * alpha)), incoming);
	drawList->AddText(nullptr, 17.0f * scale, toGameView(centerX - 115.0f, centerY + 62.0f), IM_COL32(255, 123, 133, static_cast<int>(255.0f * alpha)), "最終目標：ボスを倒せ");
#else
	(void)state;
	(void)screenWidth;
	(void)screenHeight;
#endif
}
