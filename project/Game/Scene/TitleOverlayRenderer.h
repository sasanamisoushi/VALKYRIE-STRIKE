#pragma once

class TitleScene;

// タイトル画面のメニュー・設定・アトラクトデモ案内・ワープフォールド演出の2D描画を担当する。
class TitleOverlayRenderer {
public:
	void DrawAttractDemoStartPrompt(const TitleScene& scene, float screenWidth, float screenHeight) const;
	void DrawWarpCloudLayers(const TitleScene& scene, float screenWidth, float screenHeight, int launchDurationFrames) const;
	void DrawFoldTransition(const TitleScene& scene, float screenWidth, float screenHeight, int launchDurationFrames) const;
	void DrawMenuOverlay(const TitleScene& scene, float screenWidth, float screenHeight, float opacity) const;
};
