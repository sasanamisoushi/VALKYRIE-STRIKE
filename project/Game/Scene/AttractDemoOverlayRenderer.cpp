#include "AttractDemoOverlayRenderer.h"

#include "AttractDemoState.h"
#include "engine/Debug/ImGuiManager.h"

#ifdef ENABLE_IMGUI
#include <externals/imgui/imgui.h>
#endif

#include <algorithm>
#include <cmath>

void AttractDemoOverlayRenderer::Draw(bool enabled, const AttractDemoState& demoState,
	float screenWidth, float screenHeight) const {
	if (!enabled) {
		return;
	}

#ifdef ENABLE_IMGUI
	if (!ImGui::GetCurrentContext()) {
		return;
	}

	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float width = displaySize.x > 1.0f ? displaySize.x : screenWidth;
	const float height = displaySize.y > 1.0f ? displaySize.y : screenHeight;
	const char* guide = "[ TAB ]  RETURN TO TITLE";
	ImFont* guideFont = ImGuiManager::GetGameOverFont();
	if (!guideFont) {
		guideFont = ImGui::GetFont();
	}
	constexpr float kGuideFontSize = 20.0f;
	const ImVec2 textSize = guideFont->CalcTextSizeA(kGuideFontSize, 1000.0f, 0.0f, guide);
	const float textX = (width - textSize.x) * 0.5f;
	const float textY = height - 62.0f;
	const float pulse = 0.78f + std::sin(static_cast<float>(demoState.GetFrame()) * 0.08f) * 0.16f;
	const int alpha = static_cast<int>(std::clamp(pulse, 0.0f, 1.0f) * 255.0f);
	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());

	// リザルトと同じシアン基調の小型HUDにして、戦闘画面の情報を邪魔しない位置に置く。
	drawList->AddRectFilled(
		ImVec2(textX - 24.0f, textY - 9.0f),
		ImVec2(textX + textSize.x + 24.0f, textY + textSize.y + 10.0f),
		IM_COL32(2, 14, 31, 205), 3.0f);
	drawList->AddLine(
		ImVec2(textX - 24.0f, textY - 9.0f), ImVec2(textX + 30.0f, textY - 9.0f),
		IM_COL32(80, 226, 255, alpha), 2.0f);
	drawList->AddLine(
		ImVec2(textX + textSize.x - 30.0f, textY + textSize.y + 10.0f),
		ImVec2(textX + textSize.x + 24.0f, textY + textSize.y + 10.0f),
		IM_COL32(80, 226, 255, alpha), 2.0f);
	drawList->AddText(guideFont, kGuideFontSize, ImVec2(textX + 1.5f, textY + 2.0f), IM_COL32(0, 0, 0, alpha), guide);
	drawList->AddText(guideFont, kGuideFontSize, ImVec2(textX, textY), IM_COL32(194, 246, 255, alpha), guide);
#else
	(void)demoState;
	(void)screenWidth;
	(void)screenHeight;
#endif
}
