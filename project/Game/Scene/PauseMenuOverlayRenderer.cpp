#include "PauseMenuOverlayRenderer.h"

#include "2D/Sprite.h"
#include "PauseMenuState.h"

#include <algorithm>

void PauseMenuOverlayRenderer::Draw(const PauseMenuState& state, Sprite* overlaySprite,
	Sprite* panelSprite, Sprite* labelsSprite, float screenWidth, float screenHeight) const {
	if (!state.IsPaused() || !overlaySprite || !panelSprite) {
		return;
	}

	overlaySprite->SetPosition({ 0.0f, 0.0f });
	overlaySprite->SetSize({ screenWidth, screenHeight });
	overlaySprite->SetColor({ 0.01f, 0.02f, 0.06f, 0.70f });
	overlaySprite->Update();
	overlaySprite->Draw();

	const float panelWidth = (std::min)(460.0f, screenWidth - 48.0f);
	constexpr float kPanelHeight = 324.0f;
	const float panelX = (screenWidth - panelWidth) * 0.5f;
	const float panelY = (screenHeight - kPanelHeight) * 0.5f;
	panelSprite->SetPosition({ panelX, panelY });
	panelSprite->SetSize({ panelWidth, kPanelHeight });
	panelSprite->SetColor({ 0.04f, 0.12f, 0.24f, 0.96f });
	panelSprite->Update();
	panelSprite->Draw();
	panelSprite->SetPosition({ panelX, panelY + 8.0f });
	panelSprite->SetSize({ panelWidth, 4.0f });
	panelSprite->SetColor({ 0.05f, 0.82f, 1.0f, 1.0f });
	panelSprite->Update();
	panelSprite->Draw();

	const float labelsX = panelX + 17.0f;
	const float labelsY = panelY + 18.0f;
	const float labelsWidth = panelWidth - 34.0f;
	const float labelsHeight = labelsWidth * (2.0f / 3.0f);
	const float selectedY = labelsY + (state.IsTitleSelected() ? 181.0f : 135.0f);
	const Vector4 selectedColor = { 0.08f, 0.90f, 1.0f, 0.95f };
	const Vector4 selectedGlowColor = { 0.05f, 0.40f, 0.80f, 0.78f };
	auto drawSelection = [&](float y, const Vector4& color, const Vector2& size) {
		panelSprite->SetPosition({ labelsX + 18.0f, y });
		panelSprite->SetSize(size);
		panelSprite->SetColor(color);
		panelSprite->Update();
		panelSprite->Draw();
	};
	drawSelection(selectedY, selectedGlowColor, { labelsWidth - 36.0f, 3.0f });
	drawSelection(selectedY + 40.0f, selectedColor, { labelsWidth - 36.0f, 3.0f });
	drawSelection(selectedY, selectedColor, { 4.0f, 43.0f });
	drawSelection(selectedY, selectedColor, { 36.0f, 4.0f });
	drawSelection(selectedY + 39.0f, selectedColor, { 36.0f, 4.0f });

	if (labelsSprite) {
		labelsSprite->SetPosition({ labelsX, labelsY });
		labelsSprite->SetSize({ labelsWidth, labelsHeight });
		labelsSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		labelsSprite->Update();
		labelsSprite->Draw();
	}

	drawSelection(selectedY + 13.0f, selectedColor, { 9.0f, 17.0f });
}
