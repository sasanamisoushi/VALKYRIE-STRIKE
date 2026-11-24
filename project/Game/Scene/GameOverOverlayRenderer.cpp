#include "GameOverOverlayRenderer.h"

#include "GameOverSequence.h"
#include "engine/Debug/ImGuiManager.h"

#ifdef ENABLE_IMGUI
#include <externals/imgui/imgui.h>

#include <algorithm>

namespace {
	float SmoothStep01(float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}
}
#endif

void GameOverOverlayRenderer::Draw(const GameOverSequence& sequence,
	float screenWidth, float screenHeight) const {
	if (!sequence.IsActive()) {
		return;
	}

#ifdef ENABLE_IMGUI
	if (!ImGui::GetCurrentContext()) {
		return;
	}

	constexpr int kRevealStartFrame = 30;
	constexpr int kRevealFrames = 22;
	const float reveal = std::clamp(
		static_cast<float>(sequence.GetFrame() - kRevealStartFrame) / static_cast<float>(kRevealFrames),
		0.0f, 1.0f);
	if (reveal <= 0.0f) {
		return;
	}

	const float whiteout = SmoothStep01(
		(static_cast<float>(sequence.GetFrame()) - 96.0f) /
		(static_cast<float>(GameOverSequence::kCinematicFrames) - 96.0f));
	const float alpha = reveal * (1.0f - whiteout);
	if (alpha <= 0.001f) {
		return;
	}

	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float width = displaySize.x > 1.0f ? displaySize.x : screenWidth;
	const float height = displaySize.y > 1.0f ? displaySize.y : screenHeight;
	const float pulse = 1.0f + (1.0f - reveal) * 0.075f;
	const float fontSize = 116.0f * pulse;
	const char* text = "GAME OVER";
	ImFont* gameOverFont = ImGuiManager::GetGameOverFont();
	if (!gameOverFont) {
		gameOverFont = ImGui::GetFont();
	}
	const ImVec2 textSize = gameOverFont->CalcTextSizeA(fontSize, 1000.0f, 0.0f, text);
	const float textX = (width - textSize.x) * 0.5f;
	const float textY = height * 0.465f - textSize.y * 0.5f;
	const int textAlpha = static_cast<int>(alpha * 255.0f);
	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());

	const float plateLeft = textX - 68.0f;
	const float plateRight = textX + textSize.x + 68.0f;
	const float plateTop = textY - 34.0f;
	const float plateBottom = textY + textSize.y + 45.0f;
	const ImU32 dimPink = IM_COL32(255, 46, 126, static_cast<int>(textAlpha * 0.58f));
	const ImU32 brightPink = IM_COL32(255, 125, 174, textAlpha);
	const ImU32 paleText = IM_COL32(255, 231, 240, textAlpha);
	drawList->AddRectFilled(ImVec2(plateLeft, plateTop), ImVec2(plateRight, plateBottom),
		IM_COL32(4, 7, 20, static_cast<int>(alpha * 212.0f)), 3.0f);
	drawList->AddLine(ImVec2(plateLeft, plateTop), ImVec2(plateLeft + 62.0f, plateTop), brightPink, 3.0f);
	drawList->AddLine(ImVec2(plateRight - 62.0f, plateTop), ImVec2(plateRight, plateTop), brightPink, 3.0f);
	drawList->AddLine(ImVec2(plateLeft, plateBottom), ImVec2(plateLeft + 62.0f, plateBottom), brightPink, 3.0f);
	drawList->AddLine(ImVec2(plateRight - 62.0f, plateBottom), ImVec2(plateRight, plateBottom), brightPink, 3.0f);
	drawList->AddLine(ImVec2(plateLeft, textY + textSize.y + 13.0f), ImVec2(plateRight, textY + textSize.y + 13.0f), dimPink, 1.0f);
	drawList->AddLine(ImVec2(plateLeft + 16.0f, textY - 12.0f), ImVec2(plateRight - 16.0f, textY - 12.0f), dimPink, 1.0f);

	const char* systemLabel = "[ SYSTEM FAILURE / VF-15C DESTROYED ]";
	constexpr float kSystemFontSize = 17.0f;
	const ImVec2 systemSize = ImGui::GetFont()->CalcTextSizeA(kSystemFontSize, 1000.0f, 0.0f, systemLabel);
	drawList->AddText(nullptr, kSystemFontSize, ImVec2((width - systemSize.x) * 0.5f, plateTop + 7.0f), dimPink, systemLabel);
	drawList->AddText(gameOverFont, fontSize, ImVec2(textX + 4.0f, textY + 5.0f), IM_COL32(0, 0, 0, textAlpha), text);
	drawList->AddText(gameOverFont, fontSize, ImVec2(textX - 2.0f, textY), IM_COL32(255, 10, 91, static_cast<int>(textAlpha * 0.50f)), text);
	drawList->AddText(gameOverFont, fontSize, ImVec2(textX, textY), paleText, text);
#else
	(void)screenWidth;
	(void)screenHeight;
#endif
}
