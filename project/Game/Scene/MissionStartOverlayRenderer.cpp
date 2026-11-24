#include "MissionStartOverlayRenderer.h"

#include "2D/Sprite.h"
#include "WarpExitPresentationState.h"
#include "engine/base/WinApp.h"

#include <algorithm>

namespace {
	float SmoothStep01(float value) {
		value = std::clamp(value, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}
}

void MissionStartOverlayRenderer::Draw(Sprite* sprite,
	const WarpExitPresentationState& presentation) const {
	if (!sprite || !presentation.ShouldDrawMissionStartOverlay()) {
		return;
	}

	const float progress = presentation.GetMissionStartOverlayProgress();
	const float alpha = (std::min)(
		SmoothStep01(progress / 0.12f),
		1.0f - SmoothStep01((progress - 0.94f) / 0.06f));
	if (alpha <= 0.001f) {
		return;
	}

	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const float width = (std::min)(screenWidth * 0.72f, 1120.0f);
	const float height = width * 0.5625f;
	sprite->SetPosition({ screenWidth * 0.5f, screenHeight * 0.49f });
	sprite->SetSize({ width, height });
	sprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
	sprite->Update();
	sprite->Draw();
}
