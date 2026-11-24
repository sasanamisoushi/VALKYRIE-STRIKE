#pragma once

class ReturnToTitleTransition;
class Sprite;

// タイトル帰還時のノイズと CRT 収束演出を描画する。
class ReturnToTitleOverlayRenderer {
public:
	void Draw(const ReturnToTitleTransition& transition, Sprite* overlaySprite,
		Sprite* panelSprite, float screenWidth, float screenHeight) const;
};
