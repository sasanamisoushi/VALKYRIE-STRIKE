#pragma once

class PauseMenuState;
class Sprite;

// 通常のポーズメニュー背景、選択枠、ラベル画像を描画する。
class PauseMenuOverlayRenderer {
public:
	void Draw(const PauseMenuState& state, Sprite* overlaySprite, Sprite* panelSprite,
		Sprite* labelsSprite, float screenWidth, float screenHeight) const;
};
