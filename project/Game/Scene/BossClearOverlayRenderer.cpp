#include "BossClearOverlayRenderer.h"

#include "2D/Sprite.h"
#include "BossClearEffectState.h"
#include "BossClearPhaseState.h"
#include "BossClearTimelineState.h"

#include <algorithm>

namespace {
float SmoothStep01(float value) {
	value = std::clamp(value, 0.0f, 1.0f);
	return value * value * (3.0f - 2.0f * value);
}
}

void BossClearOverlayRenderer::Draw(
	const BossClearPhaseState& phaseState,
	const BossClearTimelineState& timeline,
	const BossClearEffectState& effectState,
	float flashDurationSec,
	float missionClearDurationSec,
	Sprite* topBarSprite,
	Sprite* bottomBarSprite,
	Sprite* whiteoutSprite,
	Sprite* missionClearSprite,
	float screenWidth,
	float screenHeight) const {
	if (phaseState.Is(BossClearPhase::None)) {
		return;
	}

	// 上下のシネマティック帯を演出全体を通して表示する。
	if (topBarSprite && bottomBarSprite) {
		const float reveal = SmoothStep01(timeline.GetSequenceSeconds() / 0.30f);
		const float barHeight = screenHeight * 0.135f * reveal;
		topBarSprite->SetSize({ screenWidth, barHeight });
		topBarSprite->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		topBarSprite->SetPosition({ 0.0f, 0.0f });
		topBarSprite->Update();
		topBarSprite->Draw();
		bottomBarSprite->SetSize({ screenWidth, barHeight });
		bottomBarSprite->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		bottomBarSprite->SetPosition({ 0.0f, screenHeight - barHeight });
		bottomBarSprite->Update();
		bottomBarSprite->Draw();
	}

	// フェーズ2：フラッシュ処理（0.2〜0.5秒間画面全体を白く飛ばす）
	if (phaseState.Is(BossClearPhase::FinalExplosion) && whiteoutSprite) {
		const float flashProgress = std::clamp(timeline.GetPhaseSeconds() / flashDurationSec, 0.0f, 1.0f);
		const float flashIn = SmoothStep01(flashProgress / 0.14f);
		const float flashOut = 1.0f - SmoothStep01((flashProgress - 0.14f) / 0.86f);
		const float flashAlpha = (std::min)(flashIn, flashOut);
		if (flashAlpha > 0.001f) {
			whiteoutSprite->SetPosition({ 0.0f, 0.0f });
			whiteoutSprite->SetSize({ screenWidth, screenHeight });
			whiteoutSprite->SetColor({ 1.0f, 1.0f, 1.0f, flashAlpha });
			whiteoutSprite->Update();
			whiteoutSprite->Draw();
		}
	}

	// フェーズ3：機体が画面外へ向かうタイミングで「MISSION CLEAR」をフェードイン
	if (phaseState.Is(BossClearPhase::MissionClear) && effectState.IsMissionClearUiStarted() && missionClearSprite) {
		const float progress = std::clamp(effectState.GetMissionClearUiSeconds() / missionClearDurationSec, 0.0f, 1.0f);
		const float alpha = (std::min)(
			SmoothStep01(progress / 0.18f),
			1.0f - SmoothStep01((progress - 0.90f) / 0.10f));
		if (alpha > 0.001f) {
			const float width = (std::min)(screenWidth * 0.72f, 1120.0f);
			missionClearSprite->SetPosition({ screenWidth * 0.5f, screenHeight * 0.49f });
			missionClearSprite->SetSize({ width, width * 0.5625f });
			missionClearSprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
			missionClearSprite->Update();
			missionClearSprite->Draw();
		}
	}
}
