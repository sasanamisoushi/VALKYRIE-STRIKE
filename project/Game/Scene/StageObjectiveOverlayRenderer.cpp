#include "StageObjectiveOverlayRenderer.h"

#include "engine/Camera/FlyCamera.h"
#include "engine/Debug/ImGuiManager.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Enemy.h"

#ifdef ENABLE_IMGUI
#include <externals/imgui/imgui.h>

#include <algorithm>
#include <string>
#endif

void StageObjectiveOverlayRenderer::Draw(bool enabled, const Player* player,
	const std::list<std::unique_ptr<Enemy>>& enemies, bool isBossObjective,
	float screenWidth, float screenHeight) const {
#ifdef ENABLE_IMGUI
	if (!enabled || !ImGui::GetCurrentContext()) {
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

	constexpr float kPanelX = 24.0f;
	constexpr float kPanelWidth = 400.0f;
	const PlayerMode playerMode = player ? player->GetCurrentMode() : PlayerMode::Fighter;
	const float guideHeight = playerMode == PlayerMode::Gerwalk ? 278.0f : 256.0f;
	const float panelY = 24.0f + guideHeight + 12.0f;
	constexpr float kPanelHeight = 96.0f;

	int remainingEnemies = 0;
	if (!isBossObjective) {
		for (const auto& enemy : enemies) {
			if (enemy && !enemy->IsDead() && !enemy->IsBoss()) {
				++remainingEnemies;
			}
		}
	}

	const ImU32 accent = isBossObjective ? IM_COL32(255, 80, 80, 255) : IM_COL32(72, 232, 255, 255);
	const ImU32 panelColor = isBossObjective ? IM_COL32(43, 7, 15, 222) : IM_COL32(3, 14, 30, 222);
	const ImU32 headerColor = isBossObjective ? IM_COL32(104, 18, 27, 235) : IM_COL32(8, 54, 84, 235);
	drawList->AddRectFilled(toGameView(kPanelX, panelY), toGameView(kPanelX + kPanelWidth, panelY + kPanelHeight), panelColor, 8.0f);
	drawList->AddRect(toGameView(kPanelX, panelY), toGameView(kPanelX + kPanelWidth, panelY + kPanelHeight), accent, 8.0f, 1.5f * scale, 0);
	drawList->AddRectFilled(toGameView(kPanelX + 1.0f, panelY + 1.0f), toGameView(kPanelX + kPanelWidth - 1.0f, panelY + 30.0f), headerColor, 8.0f);
	drawList->AddLine(toGameView(kPanelX + 12.0f, panelY + 35.0f), toGameView(kPanelX + kPanelWidth - 12.0f, panelY + 35.0f), accent, 1.0f * scale);
	drawList->AddText(nullptr, 14.0f * scale, toGameView(kPanelX + 14.0f, panelY + 8.0f), IM_COL32(235, 248, 255, 255), "STAGE 1 // CLEAR OBJECTIVE");

	const char* objective = isBossObjective ? "ボスを倒せ" : "すべての敵を倒せ";
	const char* status = isBossObjective ? "FINAL TARGET: BOSS" : "BOSS 出現まで敵を殲滅";
	drawList->AddText(nullptr, 24.0f * scale, toGameView(kPanelX + 16.0f, panelY + 44.0f), accent, objective);
	drawList->AddText(nullptr, 13.0f * scale, toGameView(kPanelX + 18.0f, panelY + 73.0f), IM_COL32(185, 219, 232, 255), status);
	if (!isBossObjective) {
		const std::string count = "残り " + std::to_string(remainingEnemies) + " 機";
		const ImVec2 countSize = ImGui::CalcTextSize(count.c_str());
		drawList->AddText(nullptr, 15.0f * scale,
			toGameView(kPanelX + kPanelWidth - 18.0f - countSize.x, panelY + 71.0f),
			IM_COL32(255, 226, 112, 255), count.c_str());
	}
#else
	(void)enabled;
	(void)player;
	(void)enemies;
	(void)isBossObjective;
	(void)screenWidth;
	(void)screenHeight;
#endif
}
