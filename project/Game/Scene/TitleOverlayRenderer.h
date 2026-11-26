#pragma once

class TitleScene;

// タイトル画面のメニュー・設定・アトラクトデモ案内・ワープフォールド演出の2D描画を担当する。
class TitleOverlayRenderer {
public:
	void DrawAttractDemoStartPrompt(const TitleScene& scene, float screenWidth, float screenHeight) const;
};
