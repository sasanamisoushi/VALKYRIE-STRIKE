#include "CombatHudOverlayRenderer.h"

#include "AmmoManager.h"
#include "CombatAlertState.h"
#include "SongState.h"
#include "SpecialAttackState.h"
#include "Game/Player/Player.h"
#include "Game/base/GameSettings.h"
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
#include <limits>
#include <string>

#ifdef CG2_ENABLE_GAME_UI
namespace {
struct GameViewTransform {
	float minX = 0.0f;
	float minY = 0.0f;
	float scaleX = 1.0f;
	float scaleY = 1.0f;

	ImVec2 ToGameView(float x, float y) const {
		return ImVec2(minX + x * scaleX, minY + y * scaleY);
	}

	float MinScale() const {
		return (std::min)(scaleX, scaleY);
	}
};

void DrawBoundaryAlert(ImDrawList* hudText, const GameViewTransform& view, const CombatHudOverlayContext& ctx) {
	if (ctx.isTutorialEnabled || !ctx.player || !ctx.player->IsNearBoundary()) {
		return;
	}
	const float wallIntensity = ctx.player->IsNearWallBoundary()
		? ctx.player->GetWallBoundaryWarningIntensity() : 0.0f;
	const float ceilingIntensity = ctx.player->IsNearCeilingBoundary()
		? ctx.player->GetCeilingBoundaryWarningIntensity() : 0.0f;
	const float boundaryIntensity = std::clamp((std::max)(wallIntensity, ceilingIntensity), 0.0f, 1.0f);
	const float pulse = 0.72f + 0.28f * std::sin(static_cast<float>(ctx.gameplayElapsedFrames) * 0.17f);
	const int alertAlpha = static_cast<int>((150.0f + 105.0f * boundaryIntensity) * pulse);
	const ImU32 alertOutline = IM_COL32(255, 128, 38, alertAlpha);
	const ImU32 alertFill = IM_COL32(98, 24, 4, static_cast<int>(alertAlpha * 0.82f));
	const ImU32 alertInner = IM_COL32(255, 194, 100, static_cast<int>(alertAlpha * 0.76f));
	const float centerX = ctx.screenWidth * 0.5f;
	const float top = 20.0f;

	const ImVec2 outer[5] = {
		view.ToGameView(centerX - 72.0f, top), view.ToGameView(centerX + 72.0f, top),
		view.ToGameView(centerX + 45.0f, top + 36.0f), view.ToGameView(centerX, top + 59.0f),
		view.ToGameView(centerX - 45.0f, top + 36.0f),
	};
	const ImVec2 inner[5] = {
		view.ToGameView(centerX - 60.0f, top + 7.0f), view.ToGameView(centerX + 60.0f, top + 7.0f),
		view.ToGameView(centerX + 36.0f, top + 34.0f), view.ToGameView(centerX, top + 51.0f),
		view.ToGameView(centerX - 36.0f, top + 34.0f),
	};
	hudText->AddConvexPolyFilled(outer, 5, alertFill);
	hudText->AddPolyline(outer, 5, alertOutline, 2.0f * view.MinScale(), ImDrawFlags_Closed);
	hudText->AddPolyline(inner, 5, alertInner, 1.0f * view.MinScale(), ImDrawFlags_Closed);
	hudText->AddLine(
		view.ToGameView(centerX - 46.0f, top + 31.0f), view.ToGameView(centerX + 46.0f, top + 31.0f),
		alertInner, 1.0f * view.MinScale());

	ImFont* alertFont = ImGuiManager::GetResultRankFont();
	constexpr float alertFontSize = 23.0f;
	const ImVec2 alertTextSize = alertFont
		? alertFont->CalcTextSizeA(alertFontSize, 1000.0f, 0.0f, "ALERT")
		: ImGui::CalcTextSize("ALERT");
	hudText->AddText(
		alertFont, alertFontSize,
		view.ToGameView(centerX - alertTextSize.x * 0.5f, top + 8.0f),
		IM_COL32(255, 224, 174, alertAlpha), "ALERT");
}

void DrawIncomingAttackAlert(ImDrawList* hudText, const GameViewTransform& view, const CombatHudOverlayContext& ctx) {
	if (ctx.isTutorialEnabled || !ctx.combatAlertState || !ctx.activeCamera ||
		ctx.combatAlertState->GetEnemyAttackFrames() <= 0) {
		return;
	}
	const float fadeIn = std::clamp((90.0f - static_cast<float>(ctx.combatAlertState->GetEnemyAttackFrames())) / 8.0f, 0.0f, 1.0f);
	const float fadeOut = std::clamp(static_cast<float>(ctx.combatAlertState->GetEnemyAttackFrames()) / 15.0f, 0.0f, 1.0f);
	const int alertAlpha = static_cast<int>(255.0f * (std::min)(fadeIn, fadeOut));
	const ImU32 alertColor = IM_COL32(255, 72, 92, alertAlpha);
	const Vector3 sourceOnScreen = MyMath::WorldToScreen(
		ctx.combatAlertState->GetEnemyAttackSource(), ctx.activeCamera->GetViewProjectionMatrix(), ctx.screenWidth, ctx.screenHeight);
	const float screenCenterX = ctx.screenWidth * 0.5f;
	const float screenCenterY = ctx.screenHeight * 0.5f;
	float directionX = sourceOnScreen.x - screenCenterX;
	float directionY = sourceOnScreen.y - screenCenterY;
	// 背後の敵は投影座標が反転するため、端のインジケーターだけは正しい方角へ反転する。
	if (sourceOnScreen.z < 0.0f) {
		directionX = -directionX;
		directionY = -directionY;
	}
	float directionLength = std::sqrt(directionX * directionX + directionY * directionY);
	if (directionLength < 0.001f) {
		directionY = -1.0f;
		directionLength = 1.0f;
	}
	directionX /= directionLength;
	directionY /= directionLength;

	// 攻撃元のスクリーン方角へ伸ばして、必ず表示領域の端へ配置する。
	constexpr float alertWidth = 214.0f;
	constexpr float alertHeight = 46.0f;
	constexpr float edgeInset = 26.0f;
	const float horizontalLimit = (std::max)(1.0f, screenCenterX - alertWidth * 0.5f - edgeInset);
	const float verticalLimit = (std::max)(1.0f, screenCenterY - alertHeight * 0.5f - edgeInset);
	const float horizontalScale = std::abs(directionX) > 0.001f ? horizontalLimit / std::abs(directionX) : (std::numeric_limits<float>::max)();
	const float verticalScale = std::abs(directionY) > 0.001f ? verticalLimit / std::abs(directionY) : (std::numeric_limits<float>::max)();
	const float edgeScale = (std::min)(horizontalScale, verticalScale);
	const float alertCenterX = screenCenterX + directionX * edgeScale;
	const float alertCenterY = screenCenterY + directionY * edgeScale;
	const float alertLeft = alertCenterX - alertWidth * 0.5f;
	const float alertTop = alertCenterY - alertHeight * 0.5f;
	const float perpendicularX = -directionY;
	const float perpendicularY = directionX;

	// 中央を塞がず、攻撃元の方角にだけ出す危険インジケーター。
	hudText->AddRectFilled(
		view.ToGameView(alertLeft, alertTop), view.ToGameView(alertLeft + alertWidth, alertTop + alertHeight),
		IM_COL32(37, 4, 12, static_cast<int>(alertAlpha * 0.78f)), 4.0f);
	hudText->AddRect(
		view.ToGameView(alertLeft, alertTop), view.ToGameView(alertLeft + alertWidth, alertTop + alertHeight),
		alertColor, 4.0f, 2.0f * view.MinScale(), 0);
	hudText->AddTriangleFilled(
		view.ToGameView(alertCenterX + directionX * 29.0f, alertCenterY + directionY * 29.0f),
		view.ToGameView(alertCenterX - directionX * 18.0f + perpendicularX * 13.0f, alertCenterY - directionY * 18.0f + perpendicularY * 13.0f),
		view.ToGameView(alertCenterX - directionX * 18.0f - perpendicularX * 13.0f, alertCenterY - directionY * 18.0f - perpendicularY * 13.0f), alertColor);
	hudText->AddText(nullptr, 16.0f, view.ToGameView(alertLeft + 16.0f, alertTop + 14.0f), alertColor, "INCOMING");
}

void DrawWeaponAmmoCards(ImDrawList* hudText, const GameViewTransform& view, const CombatHudOverlayContext& ctx) {
	if (!ctx.ammoManager) {
		return;
	}
	const bool isSongActive = ctx.songState && ctx.songState->IsActive();
	const ImU32 weaponTitleColor = IM_COL32(205, 236, 255, 255);
	const ImU32 normalWeaponColor = isSongActive
		? IM_COL32(255, 230, 86, 255)
		: IM_COL32(154, 255, 69, 255);
	const ImU32 homingWeaponColor = isSongActive
		? IM_COL32(255, 230, 86, 255)
		: IM_COL32(67, 226, 255, 255);
	const float weaponPanelLeft = ctx.ammoPanelX + 18.0f;
	const float weaponPanelWidth = 385.0f;
	const float weaponPanelHeight = 112.0f;

	auto drawWeaponAmmoCard = [&](const char* slot, const char* weaponName, float top, float fillRatio, ImU32 color) {
		const float cardRight = weaponPanelLeft + weaponPanelWidth;
		const float dividerX = weaponPanelLeft + 88.0f;
		// 数値部分は透明にして、先に描いたデジタル残弾表示を鮮明に残す。
		hudText->AddRect(
			view.ToGameView(weaponPanelLeft, top), view.ToGameView(cardRight, top + weaponPanelHeight),
			color, 4.0f, 2.0f * view.MinScale(), 0);
		hudText->AddRectFilled(
			view.ToGameView(weaponPanelLeft + 5.0f, top + 5.0f), view.ToGameView(weaponPanelLeft + 77.0f, top + 28.0f),
			IM_COL32(12, 32, 54, 235), 2.0f);
		hudText->AddLine(view.ToGameView(dividerX, top + 4.0f), view.ToGameView(dividerX, top + weaponPanelHeight - 4.0f), color, 1.5f);
		hudText->AddLine(view.ToGameView(dividerX + 12.0f, top + 42.0f), view.ToGameView(cardRight - 12.0f, top + 42.0f), IM_COL32(47, 95, 118, 230), 1.0f);
		hudText->AddText(nullptr, 17.0f, view.ToGameView(weaponPanelLeft + 11.0f, top + 7.0f), color, slot);
		hudText->AddText(nullptr, 20.0f, view.ToGameView(weaponPanelLeft + 108.0f, top + 10.0f), weaponTitleColor, weaponName);
		hudText->AddText(view.ToGameView(weaponPanelLeft + 108.0f, top + 51.0f), IM_COL32(126, 181, 205, 255), "AMMUNITION");

		// 中央の8セルは、残弾の割合をひと目で分かる補助表示にする。
		constexpr int kMagazineCellCount = 8;
		const int filledCellCount = static_cast<int>(std::ceil(std::clamp(fillRatio, 0.0f, 1.0f) * kMagazineCellCount));
		for (int cellIndex = 0; cellIndex < kMagazineCellCount; ++cellIndex) {
			const float cellLeft = weaponPanelLeft + 108.0f + static_cast<float>(cellIndex) * 20.0f;
			const ImU32 cellColor = cellIndex < filledCellCount ? color : IM_COL32(33, 63, 82, 210);
			hudText->AddRectFilled(
				view.ToGameView(cellLeft, top + 77.0f), view.ToGameView(cellLeft + 14.0f, top + 86.0f), cellColor, 1.5f);
		}
	};

	drawWeaponAmmoCard(
		"MAIN", "NORMAL SHOT", ctx.normalAmmoPanelY + 20.0f,
		static_cast<float>(ctx.ammoManager->GetNormalMagazine()) /
			static_cast<float>((std::max)(ctx.ammoManager->GetNormalMagazineCapacity(), 1)),
		normalWeaponColor);
	drawWeaponAmmoCard(
		"SUB", "HOMING MISSILE", ctx.homingAmmoPanelY + 20.0f,
		static_cast<float>(ctx.ammoManager->GetHomingMagazine()) /
			static_cast<float>((std::max)(ctx.ammoManager->GetHomingMagazineCapacity(), 1)),
		homingWeaponColor);
}

void DrawReloadWarning(ImDrawList* hudText, const GameViewTransform& view, const CombatHudOverlayContext& ctx) {
	if (!ctx.combatAlertState || ctx.combatAlertState->GetReloadWarningFrames() <= 0) {
		return;
	}
	const float fadeIn = std::clamp((72.0f - static_cast<float>(ctx.combatAlertState->GetReloadWarningFrames())) / 8.0f, 0.0f, 1.0f);
	const float fadeOut = std::clamp(static_cast<float>(ctx.combatAlertState->GetReloadWarningFrames()) / 12.0f, 0.0f, 1.0f);
	const int warningAlpha = static_cast<int>(255.0f * (std::min)(fadeIn, fadeOut));
	const ImU32 warningColor = ctx.combatAlertState->IsHomingReloadWarning()
		? IM_COL32(77, 228, 255, warningAlpha)
		: IM_COL32(255, 214, 70, warningAlpha);
	const float warningWidth = 360.0f;
	const float warningHeight = 78.0f;
	const float warningLeft = (ctx.screenWidth - warningWidth) * 0.5f;
	const float warningTop = ctx.screenHeight * 0.45f;
	const char* weaponLabel = ctx.combatAlertState->IsHomingReloadWarning() ? "HOMING MISSILE" : "NORMAL SHOT";
	ImFont* reloadFont = ImGuiManager::GetResultRankFont();
	constexpr float reloadFontSize = 38.0f;
	const ImVec2 reloadSize = reloadFont
		? reloadFont->CalcTextSizeA(reloadFontSize, 1000.0f, 0.0f, "RELOADING")
		: ImGui::CalcTextSize("RELOADING");

	// 照準の中央で、発射できない理由と対象武器を一瞬で伝える警告パネル。
	hudText->AddRectFilled(
		view.ToGameView(warningLeft, warningTop), view.ToGameView(warningLeft + warningWidth, warningTop + warningHeight),
		IM_COL32(3, 16, 33, static_cast<int>(warningAlpha * 0.88f)), 4.0f);
	hudText->AddRect(
		view.ToGameView(warningLeft, warningTop), view.ToGameView(warningLeft + warningWidth, warningTop + warningHeight),
		warningColor, 4.0f, 2.0f * view.MinScale(), 0);
	hudText->AddLine(
		view.ToGameView(warningLeft + 18.0f, warningTop + 55.0f), view.ToGameView(warningLeft + warningWidth - 18.0f, warningTop + 55.0f),
		warningColor, 1.0f * view.MinScale());
	hudText->AddText(
		reloadFont, reloadFontSize,
		view.ToGameView(warningLeft + (warningWidth - reloadSize.x) * 0.5f, warningTop + 6.0f),
		warningColor, "RELOADING");
	hudText->AddText(
		view.ToGameView(warningLeft + (warningWidth - ImGui::CalcTextSize(weaponLabel).x) * 0.5f, warningTop + 59.0f),
		warningColor, weaponLabel);
}

void DrawStatusPanelOverlay(ImDrawList* hudText, const GameViewTransform& view, const CombatHudOverlayContext& ctx) {
	const bool isSongActive = ctx.songState && ctx.songState->IsActive();
	const float songGauge = ctx.songState ? ctx.songState->GetGauge() : 0.0f;
	const ImU32 songTextColor = isSongActive
		? IM_COL32(255, 95, 215, 255)
		: (songGauge >= SongState::kMaximumGauge ? IM_COL32(125, 255, 125, 255) : IM_COL32(105, 225, 240, 255));
	// 左下パネルは行を分け、数値も重ねて「どのゲージがどれだけ残っているか」を
	// 視線移動なしで把握できるようにする。
	const ImU32 statusLineColor = IM_COL32(72, 184, 226, 150);
	hudText->AddRect(
		view.ToGameView(ctx.statusPanelX + 10.0f, ctx.statusPanelY + 12.0f),
		view.ToGameView(ctx.statusPanelX + ctx.statusPanelWidth - 10.0f, ctx.statusPanelY + ctx.statusPanelHeight - 12.0f),
		statusLineColor, 5.0f, 1.0f * view.MinScale(), 0);
	hudText->AddLine(
		view.ToGameView(ctx.statusPanelX + 22.0f, ctx.statusPanelY + 66.0f),
		view.ToGameView(ctx.statusPanelX + ctx.statusPanelWidth - 22.0f, ctx.statusPanelY + 66.0f),
		statusLineColor, 1.0f * view.MinScale());
	hudText->AddLine(
		view.ToGameView(ctx.statusPanelX + 22.0f, ctx.statusPanelY + 116.0f),
		view.ToGameView(ctx.statusPanelX + ctx.statusPanelWidth - 22.0f, ctx.statusPanelY + 116.0f),
		statusLineColor, 1.0f * view.MinScale());

	auto drawStatusReadout = [&](const std::string& value, float y, ImU32 color) {
		const float x = ctx.statusPanelX + ctx.statusPanelWidth - 20.0f - static_cast<float>(value.size()) * 9.0f;
		hudText->AddText(nullptr, 16.0f, view.ToGameView(x + 1.0f, y + 1.0f), IM_COL32(0, 8, 18, 235), value.c_str());
		hudText->AddText(nullptr, 16.0f, view.ToGameView(x, y), color, value.c_str());
	};
	const int playerHp = ctx.player ? ctx.player->GetHP() : 0;
	const float spGauge = ctx.specialAttackState ? ctx.specialAttackState->GetGauge() : 0.0f;
	drawStatusReadout(std::to_string(playerHp) + " / " + std::to_string(Player::kMaxHP), ctx.statusPanelY + 33.0f, IM_COL32(255, 240, 242, 255));
	drawStatusReadout(std::to_string(static_cast<int>(std::round(spGauge))) + "%", ctx.statusPanelY + 81.0f, IM_COL32(220, 249, 255, 255));

	hudText->AddText(view.ToGameView(ctx.statusPanelX + 27.0f, ctx.songGaugeY + 1.0f), songTextColor, "SONG");
	const char* songStateLabel = isSongActive ? "LIVE" : (songGauge >= SongState::kMaximumGauge ? "READY" : "CHG");
	hudText->AddText(view.ToGameView(ctx.gaugeX + ctx.songGaugeWidth + 26.0f, ctx.songGaugeY + 1.0f), songTextColor, songStateLabel);
}

void DrawSongOverdriveOverlay(ImDrawList* hudText, const GameViewTransform& view, const CombatHudOverlayContext& ctx) {
	if (!ctx.songState || !ctx.songState->IsActive()) {
		return;
	}
	const ImU32 infiniteAmmoColor = IM_COL32(255, 230, 86, 255);
	// 数字用テクスチャには無限大記号がないため、2つのループを描いて
	// 弾数欄そのものを ∞ にする。解像度に応じてゲーム画面の座標へ変換する。
	auto drawInfiniteAmmoMarker = [&](float rightX, float y) {
		const float centerX = rightX - 27.0f;
		const float centerY = y + 15.0f;
		const float halfWidth = 24.0f;
		const float halfHeight = 14.0f;
		const float strokeWidth = 4.0f * view.MinScale();
		hudText->PathLineTo(view.ToGameView(centerX, centerY));
		hudText->PathBezierCubicCurveTo(
			view.ToGameView(centerX - 8.0f, centerY - halfHeight),
			view.ToGameView(centerX - halfWidth, centerY - halfHeight),
			view.ToGameView(centerX - halfWidth, centerY));
		hudText->PathBezierCubicCurveTo(
			view.ToGameView(centerX - halfWidth, centerY + halfHeight),
			view.ToGameView(centerX - 8.0f, centerY + halfHeight),
			view.ToGameView(centerX, centerY));
		hudText->PathBezierCubicCurveTo(
			view.ToGameView(centerX + 8.0f, centerY - halfHeight),
			view.ToGameView(centerX + halfWidth, centerY - halfHeight),
			view.ToGameView(centerX + halfWidth, centerY));
		hudText->PathBezierCubicCurveTo(
			view.ToGameView(centerX + halfWidth, centerY + halfHeight),
			view.ToGameView(centerX + 8.0f, centerY + halfHeight),
			view.ToGameView(centerX, centerY));
		hudText->PathStroke(infiniteAmmoColor, strokeWidth, 0);
	};
	drawInfiniteAmmoMarker(ctx.ammoPanelX + 400.0f, ctx.normalAmmoTextY);
	drawInfiniteAmmoMarker(ctx.ammoPanelX + 400.0f, ctx.homingAmmoTextY);

	// 既存の照準や戦闘情報を隠さない画面端・下中央だけに重ねるネオンHUD。
	const float liveTime = static_cast<float>(ctx.songState->GetFrame());
	const float livePulse = 0.5f + 0.5f * std::sin(liveTime * 0.19f);
	const ImU32 neonCyan = IM_COL32(52, 239, 255, 220);
	const ImU32 neonPink = IM_COL32(255, 66, 210, 225);
	const ImU32 neonGold = IM_COL32(255, 226, 79, 240);
	const ImU32 neonWhite = IM_COL32(240, 255, 255, 230);

	// 斜めに連なる音程ウィングを左右上部の内側へ配置する。
	const ImU32 wingColors[] = { neonCyan, neonPink, neonGold, neonPink, neonCyan };
	constexpr int kWingColorCount = 5;
	constexpr int kWingBarCount = 13;
	const float wingTop = 74.0f;
	const float wingInset = (std::max)(ctx.screenWidth * 0.095f, 130.0f);
	const float wingLength = (std::min)(ctx.screenWidth * 0.135f, 250.0f);
	const auto drawPitchWing = [&](bool isLeft) {
		for (int index = 0; index < kWingBarCount; ++index) {
			const float phase = liveTime * 0.18f + static_cast<float>(index) * 0.63f;
			const float length = wingLength * (0.52f + 0.48f * (0.5f + 0.5f * std::sin(phase)));
			const float y = wingTop + static_cast<float>(index) * 8.0f;
			const float x = isLeft
				? wingInset + static_cast<float>(index) * 12.0f
				: ctx.screenWidth - wingInset - static_cast<float>(index) * 12.0f;
			const ImU32 color = wingColors[index % kWingColorCount];
			if (isLeft) {
				hudText->AddQuadFilled(view.ToGameView(x, y), view.ToGameView(x + length, y),
					view.ToGameView(x + length - 8.0f, y + 5.0f), view.ToGameView(x - 8.0f, y + 5.0f), color);
			} else {
				hudText->AddQuadFilled(view.ToGameView(x, y), view.ToGameView(x - length, y),
					view.ToGameView(x - length + 8.0f, y + 5.0f), view.ToGameView(x + 8.0f, y + 5.0f), color);
			}
		}
	};
	drawPitchWing(true);
	drawPitchWing(false);

	const char* liveStatus = "SONG OVERDRIVE";
	const ImVec2 liveStatusSize = ImGui::CalcTextSize(liveStatus);
	const float liveStatusX = (ctx.screenWidth - liveStatusSize.x) * 0.5f;
	hudText->AddText(nullptr, 16.0f, view.ToGameView(liveStatusX + 1.0f, 23.0f), IM_COL32(10, 0, 20, 210), liveStatus);
	hudText->AddText(nullptr, 16.0f, view.ToGameView(liveStatusX, 22.0f), neonGold, liveStatus);

	// 照準の周囲にだけ置く薄いリング。
	const float centerX = ctx.screenWidth * 0.5f;
	const float centerY = ctx.screenHeight * 0.5f;
	const float centerScale = view.MinScale();
	const float ringRadius = 92.0f + livePulse * 12.0f;
	hudText->AddCircle(view.ToGameView(centerX, centerY), ringRadius * centerScale,
		IM_COL32(74, 232, 255, 82), 48, 1.2f * centerScale);
	hudText->AddCircle(view.ToGameView(centerX, centerY), (ringRadius + 18.0f) * centerScale,
		IM_COL32(255, 63, 204, 76), 48, 1.0f * centerScale);
	for (int index = 0; index < 10; ++index) {
		const float angle = liveTime * 0.042f + static_cast<float>(index) * 0.6283185f;
		const float segmentStart = ringRadius + 6.0f + (index % 2) * 15.0f;
		const float segmentEnd = segmentStart + 18.0f + livePulse * 10.0f;
		const ImU32 segmentColor = index % 2 == 0 ? neonCyan : neonPink;
		hudText->AddLine(
			view.ToGameView(centerX + std::cos(angle) * segmentStart, centerY + std::sin(angle) * segmentStart),
			view.ToGameView(centerX + std::cos(angle) * segmentEnd, centerY + std::sin(angle) * segmentEnd),
			segmentColor,
			2.5f * centerScale);
	}

	// 発動直後は大きい見出しと拡散リングを短く出す。
	if (ctx.songState->GetFrame() < 90) {
		const float activateRatio = static_cast<float>(ctx.songState->GetFrame()) / 90.0f;
		const float burstRadius = 48.0f + activateRatio * 260.0f;
		const int burstAlpha = static_cast<int>((1.0f - activateRatio) * 210.0f);
		hudText->AddCircle(view.ToGameView(centerX, centerY), burstRadius * centerScale,
			IM_COL32(255, 224, 72, burstAlpha), 64, 4.0f * centerScale);

		const char* activateText = "SONG LIVE";
		const ImVec2 activateSize = ImGui::CalcTextSize(activateText);
		const float activateX = centerX - activateSize.x * 0.5f;
		const float activateY = centerY - 74.0f - activateRatio * 20.0f;
		const int titleAlpha = static_cast<int>((1.0f - activateRatio * 0.40f) * 255.0f);
		hudText->AddText(nullptr, 30.0f, view.ToGameView(activateX + 2.0f, activateY + 3.0f),
			IM_COL32(20, 0, 32, titleAlpha), activateText);
		hudText->AddText(nullptr, 30.0f, view.ToGameView(activateX, activateY),
			IM_COL32(255, 236, 102, titleAlpha), activateText);
	}

	hudText->AddCircleFilled(view.ToGameView(centerX, centerY), 2.5f * centerScale, neonWhite);
}

void DrawControlGuide(ImDrawList* hudText, const GameViewTransform& view, const CombatHudOverlayContext& ctx) {
	if (ctx.isPaused || ctx.isGameOverActive || !GameSettings::GetInstance().IsControlGuideVisible()) {
		return;
	}
	constexpr float kGuideX = 24.0f;
	constexpr float kGuideY = 24.0f;
	constexpr float kGuideWidth = 400.0f;
	const PlayerMode playerMode = ctx.player ? ctx.player->GetCurrentMode() : PlayerMode::Fighter;
	const bool isGerwalkGuide = playerMode == PlayerMode::Gerwalk;
	const float kGuideHeight = isGerwalkGuide ? 278.0f : 256.0f;
	hudText->AddRectFilled(
		view.ToGameView(kGuideX, kGuideY),
		view.ToGameView(kGuideX + kGuideWidth, kGuideY + kGuideHeight),
		IM_COL32(3, 14, 30, 202),
		8.0f);
	hudText->AddRect(
		view.ToGameView(kGuideX, kGuideY),
		view.ToGameView(kGuideX + kGuideWidth, kGuideY + kGuideHeight),
		IM_COL32(60, 205, 245, 210),
		8.0f);
	hudText->AddRectFilled(
		view.ToGameView(kGuideX + 1.0f, kGuideY + 1.0f),
		view.ToGameView(kGuideX + kGuideWidth - 1.0f, kGuideY + 36.0f),
		IM_COL32(8, 54, 84, 210),
		8.0f);
	hudText->AddLine(
		view.ToGameView(kGuideX + 12.0f, kGuideY + 42.0f),
		view.ToGameView(kGuideX + kGuideWidth - 12.0f, kGuideY + 42.0f),
		IM_COL32(55, 175, 215, 180),
		1.0f);

	const char* modeName = isGerwalkGuide ? "GERWALK" : "FIGHTER";
	const char* lateralLabel = playerMode == PlayerMode::Gerwalk ? "左右移動" : "旋回 / 回避";
	const char* lateralKey = playerMode == PlayerMode::Gerwalk ? "A / D  STRAFE"
		: "A / D  TURN / DODGE";
	const ImU32 guideTitleColor = IM_COL32(125, 236, 255, 255);
	const ImU32 guideModeColor = IM_COL32(255, 218, 105, 255);
	const ImU32 guideLabelColor = IM_COL32(158, 207, 226, 245);
	const ImU32 guideKeyColor = IM_COL32(244, 250, 255, 255);
	constexpr float kGuideFontSize = 14.0f;
	hudText->AddText(nullptr, 16.0f, view.ToGameView(kGuideX + 14.0f, kGuideY + 10.0f), guideTitleColor, "操作方法");
	const ImVec2 modeSize = ImGui::CalcTextSize(modeName);
	hudText->AddText(nullptr, kGuideFontSize, view.ToGameView(kGuideX + kGuideWidth - 18.0f - modeSize.x, kGuideY + 12.0f), guideModeColor, modeName);

	const auto drawGuideRow = [&](float y, const char* label, const char* key) {
		hudText->AddText(nullptr, kGuideFontSize, view.ToGameView(kGuideX + 16.0f, y), guideLabelColor, label);
		hudText->AddText(nullptr, kGuideFontSize, view.ToGameView(kGuideX + 170.0f, y), guideKeyColor, key);
	};
	float guideRowY = kGuideY + 54.0f;
	const auto addGuideRow = [&](const char* label, const char* key) {
		drawGuideRow(guideRowY, label, key);
		guideRowY += 22.0f;
	};
	const bool isPad = Input::GetInstance()->IsControllerConnected();
	if (isPad) {
		addGuideRow("加速 / 減速", "RT / LT");
		if (playerMode == PlayerMode::Gerwalk) {
			addGuideRow("上昇 / 下降", "A / L3");
			addGuideRow("移動 / 視点", "Lスティック / Rスティック");
		} else {
			addGuideRow("ピッチ / ロール", "左スティック");
			addGuideRow("視点・旋回", "右スティック");
		}
		addGuideRow("通常射撃", "RB");
		addGuideRow("ホーミング", "LB長押し");
		addGuideRow("ロック切替", "R3");
		addGuideRow("歌 / 必殺技", "X / Back");
		addGuideRow("変形", "Y / 十字キー");
		if (isGerwalkGuide) {
			addGuideRow("ガード", "B");
		}
		addGuideRow("一時停止", "START");
	} else {
		addGuideRow("前進 / 後退", "W / S");
		if (playerMode == PlayerMode::Gerwalk) {
			addGuideRow("上昇 / 下降", "SPACE / SHIFT");
		} else {
			addGuideRow("機首上げ / 下げ", "マウス ↑ / ↓");
		}
		addGuideRow(lateralLabel, lateralKey);
		addGuideRow("機銃発射", "左クリック");
		addGuideRow("ミサイル照準", "右クリック長押し");
		addGuideRow("ロック / 解除", "TAB / X");
		addGuideRow("必殺技", "F");
		addGuideRow("変形", "1 / 2 / 3");
		if (isGerwalkGuide) {
			addGuideRow("ガード", "B");
		}
		addGuideRow("一時停止", "ESC");
	}
}
} // namespace
#endif

void CombatHudOverlayRenderer::Draw(const CombatHudOverlayContext& context) const {
#ifdef CG2_ENABLE_GAME_UI
	// ImGuiの座標はアプリ全体、Spriteの座標はGame View内部なので、
	// Game Viewの矩形へ変換してHUDの外へ文字がずれないようにする。
	float gameViewMinX = 0.0f;
	float gameViewMinY = 0.0f;
	float gameViewMaxX = context.screenWidth;
	float gameViewMaxY = context.screenHeight;
	const bool hasGameViewBounds = !ImGuiManager::IsVisible() ||
		FlyCamera::GetGameViewBounds(gameViewMinX, gameViewMinY, gameViewMaxX, gameViewMaxY);
	if (!ImGui::GetCurrentContext() || !hasGameViewBounds) {
		return;
	}

	ImDrawList* hudText = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
	const GameViewTransform view{
		gameViewMinX,
		gameViewMinY,
		(gameViewMaxX - gameViewMinX) / (std::max)(context.screenWidth, 1.0f),
		(gameViewMaxY - gameViewMinY) / (std::max)(context.screenHeight, 1.0f),
	};

	DrawBoundaryAlert(hudText, view, context);
	DrawIncomingAttackAlert(hudText, view, context);
	DrawWeaponAmmoCards(hudText, view, context);
	DrawReloadWarning(hudText, view, context);
	DrawStatusPanelOverlay(hudText, view, context);
	DrawSongOverdriveOverlay(hudText, view, context);
	DrawControlGuide(hudText, view, context);
#else
	(void)context;
#endif
}
