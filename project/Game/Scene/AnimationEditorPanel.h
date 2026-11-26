#pragma once

#include "3D/Animation.h"

#include <set>
#include <string>
#include <vector>

class Player;

// ボーン編集UI・タイムライン・ドラッグ編集・Undo/Redo・ショートカットを管理する。
class AnimationEditorPanel {
public:
	void RefreshActionAnimationsList();
	void DrawUI(Player* player, bool isEditorPreviewPlaying);

	const std::set<std::string>& GetSelectedBoneNames() const { return selectedBoneNames_; }
	void AddSelectedBoneName(const std::string& name) { selectedBoneNames_.insert(name); }
	void RemoveSelectedBoneName(const std::string& name) { selectedBoneNames_.erase(name); }
	void ClearSelectedBones() { selectedBoneNames_.clear(); }
	bool IsBoneSelected(const std::string& name) const { return selectedBoneNames_.count(name) > 0; }

	void AddBoneRotationFromDrag(const std::string& boneName, float deltaX, float deltaY);
	void AddBoneTranslationFromDrag(const std::string& boneName, float deltaX, float deltaY);

	void UpdateShortcuts(bool isAnimationTabActive, Player* player);
	void PushUndo();
	void Undo(Player* player);
	void Redo(Player* player);
	void DeleteSelectedBonesKeyframes();

private:
	Animation customAnimation_;
	char actionName_[128] = "Guard";
	std::vector<std::string> availableActionAnimations_;

	std::set<std::string> selectedBoneNames_;
	float currentFrameTime_ = 0.0f;
	bool isAnimationPlaying_ = false;
	bool isLooping_ = true;

	std::vector<Animation> undoHistory_;
	std::vector<Animation> redoHistory_;
	bool isDragging_ = false;
};
