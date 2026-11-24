#include "SimulationControlPanel.h"

#include "Game/enemy/Enemy.h"
#include "engine/Scene/SceneManager.h"

#ifdef ENABLE_IMGUI
#include <externals/imgui/imgui.h>
#endif

#include <algorithm>

void SimulationControlPanel::DrawEditorToolbar(const EditorToolbarContext& context) {
#ifdef ENABLE_IMGUI
	if (!ImGui::BeginMainMenuBar()) {
		return;
	}

	bool isBossBattleActive = false;
	if (context.enemies) {
		for (const auto& enemy : *context.enemies) {
			if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
				isBossBattleActive = true;
				break;
			}
		}
	}

	if (ImGui::Button("再生")) {
		// ボス戦を編集停止した後は、現在の位置・HP・設定を保ったまま再開する。
		// 通常のレベル編集では従来どおり初期配置から再生する。
		if (!isBossBattleActive && context.resetEditorPreview) {
			context.resetEditorPreview();
		}
		if (context.isEditorPreviewPlaying) {
			*context.isEditorPreviewPlaying = true;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("編集に戻す")) {
		if (isBossBattleActive) {
			// ボスを削除せず、その場の状態を保持して停止する。
			if (context.isEditorPreviewPlaying) {
				*context.isEditorPreviewPlaying = false;
			}
		} else if (context.resetEditorPreview) {
			context.resetEditorPreview();
		}
	}

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	const char* sceneLabels[] = { "タイトル", "ゲーム" };
	const char* sceneNames[] = { "TITLE", "GAMEPLAY" };
	selectedSceneIndex_ = std::clamp(selectedSceneIndex_, 0, IM_ARRAYSIZE(sceneNames) - 1);
	ImGui::SetNextItemWidth(130.0f);
	ImGui::Combo("##切替先シーン", &selectedSceneIndex_, sceneLabels, IM_ARRAYSIZE(sceneLabels));
	ImGui::SameLine();
	if (ImGui::Button("シーン切替")) {
		SceneManager::GetInstance()->ChangeScene(sceneNames[selectedSceneIndex_]);
	}

	ImGui::SameLine();
	const bool isPlaying = context.isEditorPreviewPlaying && *context.isEditorPreviewPlaying;
	ImGui::TextDisabled(isPlaying ? "再生中" :
		(isBossBattleActive ? "ボス戦編集モード（停止中）" : "編集モード（停止中）"));
	ImGui::EndMainMenuBar();
#else
	(void)context;
#endif
}

void SimulationControlPanel::DrawCategorySidebar(const char* const* categories, int categoryCount, int& selectedCategory) const {
#ifdef ENABLE_IMGUI
	if (!categories || categoryCount <= 0) {
		return;
	}
	ImGui::SetNextWindowSize(ImVec2(200.0f, 620.0f), ImGuiCond_Once);
	ImGui::Begin("設定一覧");
	ImGui::Separator();
	for (int categoryIndex = 0; categoryIndex < categoryCount; ++categoryIndex) {
		const bool isSelected = selectedCategory == categoryIndex;
		if (ImGui::Selectable(categories[categoryIndex], isSelected, 0, ImVec2(0.0f, 32.0f))) {
			selectedCategory = categoryIndex;
		}
	}
	ImGui::End();
#else
	(void)categories;
	(void)categoryCount;
	(void)selectedCategory;
#endif
}

void SimulationControlPanel::DrawDebugDisplayControls(const DebugDisplayControlsContext& context) const {
#ifdef ENABLE_IMGUI
	ImGui::Text("表示設定");
	if (context.showSkybox) ImGui::Checkbox("スカイボックスを表示", context.showSkybox);
	if (context.showPlane) ImGui::Checkbox("平面を表示", context.showPlane);
	if (context.showSphere) ImGui::Checkbox("球体（ミサイル頭）を表示", context.showSphere);
	if (context.showNormalRing) ImGui::Checkbox("通常リングを表示", context.showNormalRing);
	if (context.showPartialRing) ImGui::Checkbox("部分リングを表示", context.showPartialRing);
	if (context.showCylinder) ImGui::Checkbox("シリンダーを表示", context.showCylinder);
	if (context.showTrail) ImGui::Checkbox("トレイルを表示", context.showTrail);
	if (context.showModel) ImGui::Checkbox("モデルを表示", context.showModel);
	if (context.showParticles) ImGui::Checkbox("パーティクルを表示", context.showParticles);
	if (context.showBones) ImGui::Checkbox("ボーンを表示", context.showBones);
	if (context.showSprite) ImGui::Checkbox("スプライトを表示", context.showSprite);
	if (context.showDebugColliders) ImGui::Checkbox("Show Debug Colliders", context.showDebugColliders);
	bool showEnemyPaths = Enemy::IsPathVisualizationEnabled();
	if (ImGui::Checkbox("敵の飛行ルートを表示", &showEnemyPaths)) {
		Enemy::SetPathVisualizationEnabled(showEnemyPaths);
	}

	ImGui::Separator();
	ImGui::Text("デバック　カメラ");
	if (ImGui::Button(context.isDebugCameraActive ? "Switch to Player Camera" : "Switch to Debug Camera")) {
		if (context.setDebugCameraActive) {
			context.setDebugCameraActive(!context.isDebugCameraActive);
		}
	}
	ImGui::SameLine();
	ImGui::Text("%s", context.isDebugCameraActive ? "Active: Debug Camera" : "Active: Player Camera");
#else
	(void)context;
#endif
}
