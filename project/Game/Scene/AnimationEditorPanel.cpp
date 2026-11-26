#include "AnimationEditorPanel.h"

#include "Game/Player/Player.h"
#include "engine/math/MyMath.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <externals/imgui/imgui.h>
#include <filesystem>

void AnimationEditorPanel::RefreshActionAnimationsList() {
	availableActionAnimations_.clear();
	const std::string path = "resources/animations";
	std::error_code ec;
	if (std::filesystem::exists(path, ec)) {
		for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
			if (entry.path().extension() == ".bana") {
				availableActionAnimations_.push_back(entry.path().stem().string());
			}
		}
	}
}

void AnimationEditorPanel::DrawUI(Player* player, bool isEditorPreviewPlaying) {
#ifdef ENABLE_IMGUI
	ImGui::Text("=== アニメーション編集ツール ===");
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "[Shortcuts] Space:Play/Pause | Esc:Deselect | Del:Clear Key | Ctrl+Z/Y:Undo/Redo");

	if (!player) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "プレイヤーがいません");
		return;
	}

	ImGui::InputText("アクション名", actionName_, IM_ARRAYSIZE(actionName_));
	const std::string currentPath = "resources/animations/" + std::string(actionName_) + ".bana";

	if (ImGui::Button("バイナリからロード")) {
		if (LoadAnimationFromBinary(customAnimation_, currentPath)) {
			player->SetOverrideAnimation(&customAnimation_);
			player->SetAnimDebugActive(true);
		} else {
			OutputDebugStringA(("Failed to load animation binary: " + currentPath + "\n").c_str());
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("バイナリへ保存")) {
		if (SaveAnimationToBinary(customAnimation_, currentPath)) {
			RefreshActionAnimationsList();
		} else {
			OutputDebugStringA(("Failed to save animation binary: " + currentPath + "\n").c_str());
		}
	}

	if (!availableActionAnimations_.empty()) {
		if (ImGui::BeginCombo("既存ファイル", "")) {
			for (const auto& name : availableActionAnimations_) {
				if (ImGui::Selectable(name.c_str())) {
					strcpy_s(actionName_, name.c_str());
				}
			}
			ImGui::EndCombo();
		}
	}

	ImGui::Separator();
	ImGui::Checkbox("再生する", &isAnimationPlaying_);
	ImGui::SameLine();
	ImGui::Checkbox("ループ", &isLooping_);

	if (customAnimation_.duration <= 0.0f) {
		ImGui::Text("カスタムアニメーションがロードされていません");
		if (ImGui::Button("現在の状態から新規作成")) {
			customAnimation_ = Animation();
			customAnimation_.duration = 2.0f; // 2秒の空アニメーション
			player->SetOverrideAnimation(&customAnimation_);
			player->SetAnimDebugActive(true);
		}
		return;
	}

	ImGui::SliderFloat("全体の尺 (秒)", &customAnimation_.duration, 0.1f, 10.0f);

	// 編集モード停止中は、アニメーション編集のタイムラインも進めない。
	if (isAnimationPlaying_ && isEditorPreviewPlaying) {
		currentFrameTime_ += 1.0f / 60.0f;
		if (currentFrameTime_ > customAnimation_.duration) {
			currentFrameTime_ = isLooping_ ? 0.0f : customAnimation_.duration;
			if (!isLooping_) isAnimationPlaying_ = false;
		}
	}
	ImGui::SliderFloat("タイムライン", &currentFrameTime_, 0.0f, customAnimation_.duration, "%.3f秒");
	player->SetTargetAnimationTime(currentFrameTime_);

	ImGui::Separator();
	ImGui::Text("ボーン選択とキーフレーム編集");

	std::string selectedNamesStr;
	if (selectedBoneNames_.empty()) {
		selectedNamesStr = "(未選択)";
	} else if (selectedBoneNames_.size() == 1) {
		selectedNamesStr = *selectedBoneNames_.begin();
	} else {
		selectedNamesStr = std::to_string(selectedBoneNames_.size()) + "個のボーンを選択中";
	}

	if (ImGui::BeginCombo("編集するボーン", selectedNamesStr.c_str())) {
		for (const auto& pair : customAnimation_.nodeAnimations) {
			const bool isSelected = (selectedBoneNames_.count(pair.first) > 0);
			if (ImGui::Selectable(pair.first.c_str(), isSelected, ImGuiSelectableFlags_DontClosePopups)) {
				if (ImGui::GetIO().KeyShift) {
					if (isSelected) selectedBoneNames_.erase(pair.first);
					else selectedBoneNames_.insert(pair.first);
				} else {
					selectedBoneNames_.clear();
					selectedBoneNames_.insert(pair.first);
				}
			}
		}
		ImGui::EndCombo();
	}

	static char newBoneName[128] = "";
	ImGui::InputText("新規ボーン名", newBoneName, IM_ARRAYSIZE(newBoneName));
	ImGui::SameLine();
	if (ImGui::Button("ボーン追加")) {
		if (strlen(newBoneName) > 0) {
			customAnimation_.nodeAnimations[newBoneName] = NodeAnimation();
			selectedBoneNames_.clear();
			selectedBoneNames_.insert(newBoneName);
		}
	}

	if (!selectedBoneNames_.empty()) {
		const std::string previewName = *selectedBoneNames_.begin();
		if (customAnimation_.nodeAnimations.count(previewName)) {
			NodeAnimation& nodeAnim = customAnimation_.nodeAnimations[previewName];
			ImGui::Text("--- %s %s の編集 ---", previewName.c_str(), selectedBoneNames_.size() > 1 ? "他" : "");

			ImGui::Text("Translate (キー数: %d)", static_cast<int>(nodeAnim.translate.keyframes.size()));
			static float tVal[3] = { 0, 0, 0 };
			ImGui::DragFloat3("位置", tVal, 0.01f);
			if (ImGui::Button("現在時刻にTranslateキーを打つ")) {
				KeyframeVector3 kf;
				kf.time = currentFrameTime_;
				kf.value = { tVal[0], tVal[1], tVal[2] };
				nodeAnim.translate.keyframes.push_back(kf);
				std::sort(nodeAnim.translate.keyframes.begin(), nodeAnim.translate.keyframes.end(),
					[](const auto& a, const auto& b) { return a.time < b.time; });
			}

			ImGui::Text("Rotate (キー数: %d)", static_cast<int>(nodeAnim.rotate.keyframes.size()));
			static float rVal[3] = { 0, 0, 0 };
			ImGui::DragFloat3("回転 (XYZ度)", rVal, 1.0f);
			if (ImGui::Button("現在時刻にRotateキーを打つ")) {
				KeyframeQuaternion kf;
				kf.time = currentFrameTime_;
				Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, rVal[0] * 3.141592f / 180.0f);
				Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, rVal[1] * 3.141592f / 180.0f);
				Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, rVal[2] * 3.141592f / 180.0f);
				kf.value = MyMath::Normalize(MyMath::Multiply(MyMath::Multiply(qYaw, qPitch), qRoll));
				nodeAnim.rotate.keyframes.push_back(kf);
				std::sort(nodeAnim.rotate.keyframes.begin(), nodeAnim.rotate.keyframes.end(),
					[](const auto& a, const auto& b) { return a.time < b.time; });
			}

			ImGui::Text("Scale (キー数: %d)", static_cast<int>(nodeAnim.scale.keyframes.size()));
			static float sVal[3] = { 1, 1, 1 };
			ImGui::DragFloat3("スケール", sVal, 0.01f);
			if (ImGui::Button("現在時刻にScaleキーを打つ")) {
				KeyframeVector3 kf;
				kf.time = currentFrameTime_;
				kf.value = { sVal[0], sVal[1], sVal[2] };
				nodeAnim.scale.keyframes.push_back(kf);
				std::sort(nodeAnim.scale.keyframes.begin(), nodeAnim.scale.keyframes.end(),
					[](const auto& a, const auto& b) { return a.time < b.time; });
			}
		}
	}
#else
	(void)player;
	(void)isEditorPreviewPlaying;
#endif
}

void AnimationEditorPanel::AddBoneRotationFromDrag(const std::string& boneName, float deltaX, float deltaY) {
	if (boneName.empty()) return;

	if (customAnimation_.nodeAnimations.count(boneName) == 0) {
		customAnimation_.nodeAnimations[boneName] = NodeAnimation();
	}

	NodeAnimation& nodeAnim = customAnimation_.nodeAnimations[boneName];

	Quaternion currentRot = { 0.0f, 0.0f, 0.0f, 1.0f };
	if (!nodeAnim.rotate.keyframes.empty()) {
		currentRot = CalculateValue(nodeAnim.rotate.keyframes, currentFrameTime_);
	}

	const float speed = 0.01f;
	const Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, deltaX * speed);
	const Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, deltaY * speed);
	const Quaternion deltaRot = MyMath::Multiply(qYaw, qPitch);
	currentRot = MyMath::Normalize(MyMath::Multiply(currentRot, deltaRot));

	bool found = false;
	for (auto& kf : nodeAnim.rotate.keyframes) {
		if (std::abs(kf.time - currentFrameTime_) < 0.001f) {
			kf.value = currentRot;
			found = true;
			break;
		}
	}

	if (!found) {
		KeyframeQuaternion newKf;
		newKf.time = currentFrameTime_;
		newKf.value = currentRot;
		nodeAnim.rotate.keyframes.push_back(newKf);
		std::sort(nodeAnim.rotate.keyframes.begin(), nodeAnim.rotate.keyframes.end(),
			[](const auto& a, const auto& b) { return a.time < b.time; });
	}
}

void AnimationEditorPanel::AddBoneTranslationFromDrag(const std::string& boneName, float deltaX, float deltaY) {
	if (boneName.empty()) return;

	if (customAnimation_.nodeAnimations.count(boneName) == 0) {
		customAnimation_.nodeAnimations[boneName] = NodeAnimation();
	}

	NodeAnimation& nodeAnim = customAnimation_.nodeAnimations[boneName];

	Vector3 currentTrans = { 0.0f, 0.0f, 0.0f };
	if (!nodeAnim.translate.keyframes.empty()) {
		currentTrans = CalculateValue(nodeAnim.translate.keyframes, currentFrameTime_);
	}

	const float speed = 0.05f;
	currentTrans.x += deltaX * speed;
	currentTrans.y -= deltaY * speed;

	bool found = false;
	for (auto& kf : nodeAnim.translate.keyframes) {
		if (std::abs(kf.time - currentFrameTime_) < 0.001f) {
			kf.value = currentTrans;
			found = true;
			break;
		}
	}

	if (!found) {
		KeyframeVector3 newKf;
		newKf.time = currentFrameTime_;
		newKf.value = currentTrans;
		nodeAnim.translate.keyframes.push_back(newKf);
		std::sort(nodeAnim.translate.keyframes.begin(), nodeAnim.translate.keyframes.end(),
			[](const auto& a, const auto& b) { return a.time < b.time; });
	}
}

void AnimationEditorPanel::UpdateShortcuts(bool isAnimationTabActive, Player* player) {
#ifdef ENABLE_IMGUI
	if (!isAnimationTabActive) return;

	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		if (!isDragging_ && !selectedBoneNames_.empty()) {
			isDragging_ = true;
			PushUndo();
		}
	} else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		if (isDragging_) {
			isDragging_ = false;
		}
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Space) && !io.WantTextInput) {
		isAnimationPlaying_ = !isAnimationPlaying_;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		selectedBoneNames_.clear();
	}

	if ((ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && !io.WantTextInput) {
		PushUndo();
		DeleteSelectedBonesKeyframes();
	}

	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
		Undo(player);
	}

	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
		Redo(player);
	}
#else
	(void)isAnimationTabActive;
	(void)player;
#endif
}

void AnimationEditorPanel::PushUndo() {
	undoHistory_.push_back(customAnimation_);
	if (undoHistory_.size() > 50) {
		undoHistory_.erase(undoHistory_.begin());
	}
	redoHistory_.clear();
}

void AnimationEditorPanel::Undo(Player* player) {
	if (undoHistory_.empty()) return;

	redoHistory_.push_back(customAnimation_);
	customAnimation_ = undoHistory_.back();
	undoHistory_.pop_back();

	if (player) {
		player->SetOverrideAnimation(&customAnimation_);
	}
}

void AnimationEditorPanel::Redo(Player* player) {
	if (redoHistory_.empty()) return;

	undoHistory_.push_back(customAnimation_);
	customAnimation_ = redoHistory_.back();
	redoHistory_.pop_back();

	if (player) {
		player->SetOverrideAnimation(&customAnimation_);
	}
}

void AnimationEditorPanel::DeleteSelectedBonesKeyframes() {
	for (const auto& boneName : selectedBoneNames_) {
		if (customAnimation_.nodeAnimations.count(boneName)) {
			auto& keys = customAnimation_.nodeAnimations[boneName].rotate.keyframes;
			for (auto it = keys.begin(); it != keys.end(); ) {
				if (std::abs(it->time - currentFrameTime_) < 0.001f) {
					it = keys.erase(it);
				} else {
					++it;
				}
			}
		}
	}
}
