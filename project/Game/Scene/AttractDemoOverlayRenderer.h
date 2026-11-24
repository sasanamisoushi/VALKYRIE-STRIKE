#pragma once

class AttractDemoState;

// アトラクトデモ中のタイトル帰還ガイド HUD を描画する。
class AttractDemoOverlayRenderer {
public:
	void Draw(bool enabled, const AttractDemoState& demoState,
		float screenWidth, float screenHeight) const;
};
