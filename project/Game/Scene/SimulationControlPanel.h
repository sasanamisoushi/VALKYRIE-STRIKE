#pragma once

#include <functional>
#include <list>
#include <memory>

class Enemy;

// 上部エディタツールバー（再生／停止・シーン切替）描画に必要なデータ。
struct EditorToolbarContext {
	const std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	bool* isEditorPreviewPlaying = nullptr;
	std::function<void()> resetEditorPreview;
};

// 表示設定・デバッグ表示トグル描画に必要なデータ。
struct DebugDisplayControlsContext {
	bool* showSkybox = nullptr;
	bool* showPlane = nullptr;
	bool* showSphere = nullptr;
	bool* showNormalRing = nullptr;
	bool* showPartialRing = nullptr;
	bool* showCylinder = nullptr;
	bool* showTrail = nullptr;
	bool* showModel = nullptr;
	bool* showParticles = nullptr;
	bool* showBones = nullptr;
	bool* showSprite = nullptr;
	bool* showDebugColliders = nullptr;
	bool isDebugCameraActive = false;
	std::function<void(bool)> setDebugCameraActive;
};

// ImGui 上部ツールバー（再生／停止・シーン切替）、設定カテゴリ切替（対象切替）、
// およびデバッグ表示トグル群を描画する。
class SimulationControlPanel {
public:
	void DrawEditorToolbar(const EditorToolbarContext& context);
	void DrawCategorySidebar(const char* const* categories, int categoryCount, int& selectedCategory) const;
	void DrawDebugDisplayControls(const DebugDisplayControlsContext& context) const;

private:
	int selectedSceneIndex_ = 1;
};
