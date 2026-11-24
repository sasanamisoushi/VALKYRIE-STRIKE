#include "ReturnToTitleOverlayRenderer.h"

#include "2D/Sprite.h"
#include "ReturnToTitleTransition.h"

#include <algorithm>
#include <cmath>

void ReturnToTitleOverlayRenderer::Draw(const ReturnToTitleTransition& transition,
	Sprite* overlaySprite, Sprite* panelSprite, float screenWidth, float screenHeight) const {
	if (!overlaySprite || !panelSprite) {
		return;
	}

	const float progress = transition.GetProgress();
	auto drawBox = [panelSprite](float x, float y, float width, float height, const Vector4& color) {
		if (width <= 0.0f || height <= 0.0f) {
			return;
		}
		panelSprite->SetPosition({ x, y });
		panelSprite->SetSize({ width, height });
		panelSprite->SetColor(color);
		panelSprite->Update();
		panelSprite->Draw();
	};

	if (progress >= 0.95f) {
		drawBox(0.0f, 0.0f, screenWidth, screenHeight, { 0.0f, 0.0f, 0.0f, 1.0f });
		return;
	}

	if (progress < 0.28f) {
		const float burstT = progress / 0.28f;
		const float alpha = 1.0f - burstT * 0.4f;
		for (int index = 0; index < 7; ++index) {
			const float seed = static_cast<float>((transition.GetFrame() * 17 + index * 37) % 100) / 100.0f;
			const float bandY = seed * screenHeight;
			const float bandHeight = 3.0f + static_cast<float>((transition.GetFrame() * 7 + index * 19) % 20);
			const float bandWidth = screenWidth * (0.35f + 0.65f * static_cast<float>((index * 13) % 10) / 10.0f);
			const float bandX = static_cast<float>((transition.GetFrame() * 29 + index * 47) %
				static_cast<int>((std::max)(screenWidth - bandWidth, 1.0f)));
			const Vector4 bandColor = (index % 2 == 0)
				? Vector4{ 0.05f, 0.85f, 1.0f, 0.45f * alpha }
				: Vector4{ 1.0f, 0.25f, 0.15f, 0.35f * alpha };
			drawBox(bandX, bandY, bandWidth, bandHeight, bandColor);
		}
	}

	const float centerY = screenHeight * 0.5f;
	const float centerX = screenWidth * 0.5f;
	const float verticalProgress = std::clamp((progress - 0.18f) / 0.54f, 0.0f, 1.0f);
	const float verticalOpen = (std::max)(std::pow(1.0f - verticalProgress, 2.5f), 0.003f);
	const float halfHeight = screenHeight * 0.5f * verticalOpen;
	const float horizontalProgress = std::clamp((progress - 0.72f) / 0.22f, 0.0f, 1.0f);
	const float horizontalOpen = std::pow(1.0f - horizontalProgress, 3.0f);
	const float halfWidth = screenWidth * 0.5f * horizontalOpen;

	const float topShutterHeight = (std::max)(centerY - halfHeight, 0.0f);
	if (topShutterHeight > 0.0f) {
		drawBox(0.0f, 0.0f, screenWidth, topShutterHeight, { 0.0f, 0.0f, 0.0f, 1.0f });
	}
	const float bottomShutterY = centerY + halfHeight;
	const float bottomShutterHeight = screenHeight - bottomShutterY;
	if (bottomShutterHeight > 0.0f) {
		drawBox(0.0f, bottomShutterY, screenWidth, bottomShutterHeight, { 0.0f, 0.0f, 0.0f, 1.0f });
	}

	if (horizontalProgress > 0.0f) {
		const float leftShutterWidth = (std::max)(centerX - halfWidth, 0.0f);
		if (leftShutterWidth > 0.0f) {
			drawBox(0.0f, centerY - halfHeight, leftShutterWidth, halfHeight * 2.0f, { 0.0f, 0.0f, 0.0f, 1.0f });
		}
		const float rightShutterX = centerX + halfWidth;
		const float rightShutterWidth = screenWidth - rightShutterX;
		if (rightShutterWidth > 0.0f) {
			drawBox(rightShutterX, centerY - halfHeight, rightShutterWidth, halfHeight * 2.0f, { 0.0f, 0.0f, 0.0f, 1.0f });
		}
	}

	if (verticalProgress > 0.05f && progress < 0.94f) {
		const float lineAlpha = (std::min)(verticalProgress * 1.5f, 1.0f) *
			(1.0f - std::clamp((progress - 0.90f) / 0.04f, 0.0f, 1.0f));
		const float lineWidth = horizontalProgress > 0.0f ? halfWidth * 2.0f : screenWidth;
		const float lineX = horizontalProgress > 0.0f ? centerX - halfWidth : 0.0f;
		drawBox(lineX, centerY - 3.5f, lineWidth, 7.0f, { 0.2f, 0.8f, 1.0f, 0.65f * lineAlpha });
		drawBox(lineX, centerY - 1.0f, lineWidth, 2.0f, { 1.0f, 1.0f, 1.0f, 0.95f * lineAlpha });

		if (horizontalProgress > 0.5f) {
			const float sparkProgress = (horizontalProgress - 0.5f) / 0.5f;
			const float sparkSize = (1.0f - sparkProgress) * 16.0f + 4.0f;
			const float sparkAlpha = (1.0f - sparkProgress) * lineAlpha;
			drawBox(centerX - sparkSize * 0.5f, centerY - sparkSize * 0.5f, sparkSize, sparkSize,
				{ 1.0f, 1.0f, 1.0f, sparkAlpha });
			drawBox(centerX - sparkSize, centerY - sparkSize, sparkSize * 2.0f, sparkSize * 2.0f,
				{ 0.2f, 0.85f, 1.0f, 0.5f * sparkAlpha });
		}
	}
}
