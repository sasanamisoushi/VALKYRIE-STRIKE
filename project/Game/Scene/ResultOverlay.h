#pragma once

#include "Game/Scene/ResultData.h"
#include "engine/Debug/ImGuiManager.h"
#include <externals/imgui/imgui.h>
#include <algorithm>
#include <cstdio>

namespace ResultOverlay {

inline int CalculateBattleScore(const MissionResultData& result) {
	return result.defeatedEnemyCount * 1500;
}

inline int CalculateSurvivalScore(const MissionResultData& result) {
	// 生還できたミッション自体の達成点。装甲残量は下の ARMOR BONUS だけで評価する。
	return result.outcome == MissionResultOutcome::Clear ? 4500 : 0;
}

inline int CalculateArmorBonus(const MissionResultData& result) {
	// HP 最大値は5。残した装甲1につき1000点を加算する。
	return (std::clamp)(result.remainingHp, 0, 5) * 1000;
}

inline const char* CalculateRank(int totalScore, MissionResultOutcome outcome) {
	if (outcome == MissionResultOutcome::Failed) {
		return totalScore >= 6000 ? "C" : "D";
	}
	if (totalScore >= 18000) return "SS";
	if (totalScore >= 14500) return "S";
	if (totalScore >= 11000) return "A";
	if (totalScore >= 8000) return "B";
	return "C";
}

inline float CalculateRankGaugeTarget(const char* rank) {
	if (rank[0] == 'D') return 0.10f;
	if (rank[0] == 'C') return 0.28f;
	if (rank[0] == 'B') return 0.45f;
	if (rank[0] == 'A') return 0.62f;
	if (rank[0] == 'S' && rank[1] == 'S') return 0.96f;
	return 0.80f;
}

inline float ClampAnimation(int frame, int startFrame, int durationFrames) {
	return std::clamp(static_cast<float>(frame - startFrame) / static_cast<float>(durationFrames), 0.0f, 1.0f);
}

// uiMaskAlpha はポストエフェクトの外側に描かれるリザルトUIだけを先に消すための黒幕。
inline void Draw(const MissionResultData& result, int animationFrame, float uiMaskAlpha = 0.0f) {
	if (ImGui::GetCurrentContext() == nullptr) {
		return;
	}

	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float width = displaySize.x;
	const float height = displaySize.y;
	if (width <= 1.0f || height <= 1.0f) {
		return;
	}

	const bool isClear = result.outcome == MissionResultOutcome::Clear;
	const ImU32 accent = isClear ? IM_COL32(66, 235, 255, 255) : IM_COL32(255, 96, 148, 255);
	const ImU32 accentDim = isClear ? IM_COL32(22, 110, 150, 210) : IM_COL32(130, 30, 75, 220);
	const ImU32 panelColor = IM_COL32(2, 16, 37, 205);
	const ImU32 valueColor = IM_COL32(240, 250, 255, 255);
	const ImU32 mutedColor = IM_COL32(156, 202, 225, 255);
	// 背景のシアン／失敗時のピンクどちらにも埋もれない、見出し専用の高コントラスト色。
	const ImU32 titleColor = IM_COL32(255, 239, 154, 255);
	const int battleScore = CalculateBattleScore(result);
	const int survivalScore = CalculateSurvivalScore(result);
	const int armorBonus = CalculateArmorBonus(result);
	const int totalScore = battleScore + survivalScore + armorBonus;
	const char* rank = CalculateRank(totalScore, result.outcome);
	const float scoreAnimation = ClampAnimation(animationFrame, 4, 42);
	const float rankGaugeAnimation = ClampAnimation(animationFrame, 54, 112);
	const bool isRankRevealed = rankGaugeAnimation >= 1.0f;
	const int displayedBattleScore = static_cast<int>(battleScore * scoreAnimation);
	const int displayedSurvivalScore = static_cast<int>(survivalScore * scoreAnimation);
	const int displayedArmorBonus = static_cast<int>(armorBonus * scoreAnimation);
	const int displayedTotalScore = displayedBattleScore + displayedSurvivalScore + displayedArmorBonus;

	const float contentLeft = width * 0.14f;
	const float contentRight = width * 0.86f;
	const float headerY = height * 0.105f;
	const float panelTop = height * 0.245f;
	const float panelBottom = height * 0.745f;
	const float scorePanelRight = width * 0.60f;
	const float rankPanelLeft = width * 0.64f;

	// 上下の走査線とヘッダーで、飛行機HUDと同じ情報パネルらしさを作る。
	drawList->AddLine(ImVec2(contentLeft, headerY + 40.0f), ImVec2(contentRight, headerY + 40.0f), accent, 2.0f);
	drawList->AddLine(ImVec2(contentLeft, headerY + 46.0f), ImVec2(contentRight - 130.0f, headerY + 46.0f), accentDim, 1.0f);
	// 見出しは背景の発光線と重ならない暗いプレートを敷いて可読性を確保する。
	drawList->AddRectFilled(ImVec2(contentLeft - 12.0f, headerY - 5.0f), ImVec2(contentLeft + 286.0f, headerY + 37.0f), IM_COL32(1, 13, 31, 210), 3.0f);
	drawList->AddText(nullptr, 34.0f, ImVec2(contentLeft + 2.0f, headerY + 2.0f), IM_COL32(0, 7, 18, 255), "MISSION RESULT");
	drawList->AddText(nullptr, 34.0f, ImVec2(contentLeft, headerY), titleColor, "MISSION RESULT");
	// 失敗リザルトではミッション失敗よりも先に、ゲームオーバーであることを明確に伝える。
	const char* outcomeLabel = isClear ? "MISSION COMPLETE" : "GAME OVER";
	const float outcomeFontSize = isClear ? 18.0f : 26.0f;
	const ImVec2 outcomeSize = ImGui::GetFont()->CalcTextSizeA(outcomeFontSize, 1000.0f, 0.0f, outcomeLabel);
	drawList->AddText(nullptr, outcomeFontSize, ImVec2(contentRight - outcomeSize.x, headerY + (isClear ? 11.0f : 6.0f)), accent, outcomeLabel);

	drawList->AddRectFilled(ImVec2(contentLeft, panelTop), ImVec2(scorePanelRight, panelBottom), panelColor, 5.0f);
	drawList->AddRect(ImVec2(contentLeft, panelTop), ImVec2(scorePanelRight, panelBottom), accentDim, 5.0f, 0, 2.0f);
	drawList->AddRectFilled(ImVec2(rankPanelLeft, panelTop), ImVec2(contentRight, panelBottom), panelColor, 5.0f);
	drawList->AddRect(ImVec2(rankPanelLeft, panelTop), ImVec2(contentRight, panelBottom), accentDim, 5.0f, 0, 2.0f);

	const float statLeft = contentLeft + 28.0f;
	const float statRight = scorePanelRight - 28.0f;
	auto drawScoreLine = [&](const char* label, int score, float y, float ratio) {
		char scoreText[32]{};
		std::snprintf(scoreText, sizeof(scoreText), "%06d", score);
		drawList->AddText(nullptr, 17.0f, ImVec2(statLeft, y), mutedColor, label);
		drawList->AddText(nullptr, 24.0f, ImVec2(statRight - 96.0f, y - 4.0f), valueColor, scoreText);
		drawList->AddRectFilled(ImVec2(statLeft, y + 32.0f), ImVec2(statRight, y + 39.0f), IM_COL32(9, 48, 77, 230), 2.0f);
		drawList->AddRectFilled(ImVec2(statLeft, y + 32.0f), ImVec2(statLeft + (statRight - statLeft) * std::clamp(ratio, 0.0f, 1.0f), y + 39.0f), accent, 2.0f);
	};

	drawScoreLine("BATTLE SCORE", displayedBattleScore, panelTop + 42.0f, scoreAnimation * static_cast<float>(battleScore) / 12000.0f);
	drawScoreLine("MISSION SCORE", displayedSurvivalScore, panelTop + 132.0f, scoreAnimation * static_cast<float>(survivalScore) / 4500.0f);
	drawScoreLine("ARMOR BONUS", displayedArmorBonus, panelTop + 222.0f, scoreAnimation * static_cast<float>(armorBonus) / 5000.0f);
	drawList->AddLine(ImVec2(statLeft, panelTop + 321.0f), ImVec2(statRight, panelTop + 321.0f), accent, 2.0f);
	drawList->AddText(nullptr, 20.0f, ImVec2(statLeft, panelTop + 340.0f), accent, "TOTAL SCORE");
	char totalText[32]{};
	std::snprintf(totalText, sizeof(totalText), "%06d", displayedTotalScore);
	drawList->AddText(nullptr, 32.0f, ImVec2(statRight - 132.0f, panelTop + 328.0f), valueColor, totalText);

	drawList->AddText(nullptr, 18.0f, ImVec2(rankPanelLeft + 24.0f, panelTop + 28.0f), mutedColor, "MISSION RANK");
	// ランクゲージは集計後に左から動き、該当ランクで止まる。
	const float gaugeLeft = rankPanelLeft + 28.0f;
	const float gaugeRight = contentRight - 28.0f;
	const float gaugeY = panelTop + 102.0f;
	const float gaugeWidth = gaugeRight - gaugeLeft;
	const char* gaugeLabels[] = { "D", "C", "B", "A", "S", "SS" };
	drawList->AddText(nullptr, 15.0f, ImVec2(gaugeLeft, gaugeY - 29.0f), mutedColor,
		isRankRevealed ? "RANK CONFIRMED" : "CALCULATING RANK...");
	drawList->AddRectFilled(ImVec2(gaugeLeft, gaugeY), ImVec2(gaugeRight, gaugeY + 12.0f), IM_COL32(9, 48, 77, 240), 3.0f);
	const float gaugeProgress = CalculateRankGaugeTarget(rank) * rankGaugeAnimation;
	drawList->AddRectFilled(ImVec2(gaugeLeft, gaugeY), ImVec2(gaugeLeft + gaugeWidth * gaugeProgress, gaugeY + 12.0f), accent, 3.0f);
	for (int index = 0; index < 6; ++index) {
		const float tick = static_cast<float>(index) / 5.0f;
		const float tickX = gaugeLeft + gaugeWidth * tick;
		const ImU32 labelColor = gaugeProgress + 0.015f >= tick ? accent : mutedColor;
		drawList->AddLine(ImVec2(tickX, gaugeY - 5.0f), ImVec2(tickX, gaugeY + 17.0f), accentDim, 1.0f);
		const ImVec2 labelSize = ImGui::CalcTextSize(gaugeLabels[index]);
		drawList->AddText(nullptr, 16.0f, ImVec2(tickX - labelSize.x * 0.5f, gaugeY + 23.0f), labelColor, gaugeLabels[index]);
	}
	const float markerX = gaugeLeft + gaugeWidth * gaugeProgress;
	drawList->AddTriangleFilled(ImVec2(markerX, gaugeY - 14.0f), ImVec2(markerX - 7.0f, gaugeY - 4.0f), ImVec2(markerX + 7.0f, gaugeY - 4.0f), accent);

	if (isRankRevealed) {
		ImFont* rankFont = ImGuiManager::GetResultRankFont();
		const float rankFontSize = 92.0f;
		const ImVec2 rankSize = rankFont
			? rankFont->CalcTextSizeA(rankFontSize, 1000.0f, 0.0f, rank)
			: ImGui::CalcTextSize(rank);
		const ImVec2 rankPosition((rankPanelLeft + contentRight - rankSize.x) * 0.5f, panelTop + 143.0f);
		// 薄い外側のグローと高解像度フォント本体を重ね、にじまず発光して見えるようにする。
		drawList->AddText(rankFont, rankFontSize, ImVec2(rankPosition.x + 2.0f, rankPosition.y + 3.0f), IM_COL32(0, 15, 32, 220), rank);
		drawList->AddText(rankFont, rankFontSize, rankPosition, accent, rank);
	}
	drawList->AddLine(ImVec2(rankPanelLeft + 24.0f, panelTop + 242.0f), ImVec2(contentRight - 24.0f, panelTop + 242.0f), accentDim, 1.0f);
	char destroyText[48]{};
	std::snprintf(destroyText, sizeof(destroyText), "DESTROYED     %02d", result.defeatedEnemyCount);
	drawList->AddText(nullptr, 18.0f, ImVec2(rankPanelLeft + 26.0f, panelTop + 268.0f), valueColor, destroyText);
	char hpText[48]{};
	std::snprintf(hpText, sizeof(hpText), "ARMOR LEFT    %03d", (std::max)(0, result.remainingHp));
	drawList->AddText(nullptr, 18.0f, ImVec2(rankPanelLeft + 26.0f, panelTop + 303.0f), valueColor, hpText);
	const int elapsedSeconds = (std::max)(0, result.elapsedFrames) / 60;
	char timeText[48]{};
	std::snprintf(timeText, sizeof(timeText), "MISSION TIME  %02d:%02d", elapsedSeconds / 60, elapsedSeconds % 60);
	drawList->AddText(nullptr, 18.0f, ImVec2(rankPanelLeft + 26.0f, panelTop + 338.0f), valueColor, timeText);

	const char* footer = "[ SPACE ]  RETURN TO TITLE";
	constexpr float footerFontSize = 28.0f;
	const ImVec2 footerSize = ImGui::GetFont()->CalcTextSizeA(footerFontSize, 1000.0f, 0.0f, footer);
	const ImVec2 footerMin((width - footerSize.x) * 0.5f - 32.0f, height * 0.835f);
	const ImVec2 footerMax((width + footerSize.x) * 0.5f + 32.0f, height * 0.835f + 48.0f);
	drawList->AddRectFilled(footerMin, footerMax, IM_COL32(1, 14, 33, 228), 4.0f);
	drawList->AddRect(footerMin, footerMax, accentDim, 4.0f, 0, 2.0f);
	drawList->AddText(nullptr, footerFontSize, ImVec2((width - footerSize.x) * 0.5f, footerMin.y + 10.0f), valueColor, footer);

	// ImGuiのリザルトUIはポストエフェクトの後段に描かれる。シャットダウン中に
	// ランクや数値が静止したまま残らないよう、UIの各領域だけを先に覆う。
	// 背景は覆わないため、裏側のCRT収束・ノイズ演出は最後まで見える。
	const float clampedUiMask = std::clamp(uiMaskAlpha, 0.0f, 1.0f);
	if (clampedUiMask > 0.0f) {
		const ImU32 maskColor = IM_COL32(0, 0, 0, static_cast<int>(clampedUiMask * 255.0f));
		drawList->AddRectFilled(ImVec2(contentLeft - 16.0f, headerY - 9.0f), ImVec2(contentRight + 8.0f, headerY + 50.0f), maskColor, 4.0f);
		drawList->AddRectFilled(ImVec2(contentLeft - 2.0f, panelTop - 2.0f), ImVec2(scorePanelRight + 2.0f, panelBottom + 2.0f), maskColor, 5.0f);
		drawList->AddRectFilled(ImVec2(rankPanelLeft - 2.0f, panelTop - 2.0f), ImVec2(contentRight + 2.0f, panelBottom + 2.0f), maskColor, 5.0f);
		drawList->AddRectFilled(
			ImVec2(footerMin.x - 3.0f, footerMin.y - 3.0f),
			ImVec2(footerMax.x + 3.0f, footerMax.y + 3.0f), maskColor, 5.0f);
	}
}

} // namespace ResultOverlay
