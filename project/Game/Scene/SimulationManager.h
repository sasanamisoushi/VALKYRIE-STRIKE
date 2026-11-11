#pragma once

#include <string>
#include <vector>
#include "engine/Camera/Camera.h"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/bullet/Missile.h"
#include "externals/json.hpp"

#include <set>

class GamePlayScene;

class SimulationManager {
public:
	SimulationManager(GamePlayScene* scene);
	bool SaveCurrentSimulationLayoutToSceneJson(const std::string &filePath);
	// ゲーム内レベルエディタで作成した敵を、Blender と共通の scene.json 形式で保存する。
	bool AddEnemySpawnToSceneJson(
		const std::string &filePath,
		const std::string &name,
		const std::string &enemyType,
		const Vector3 &position,
		const Vector3 &rotation);
	// scene.json 内の敵オブジェクトの並びを入れ替え、初期スポーン順を変更する。
	bool MoveEnemySpawnInSceneJson(const std::string &filePath, size_t sourceIndex, size_t targetIndex);
	void RefreshSimulationActionNames();
	bool SaveNamedSimulationAction(const std::string &filePath, const std::string &actionName);
	bool ApplySimulationAction(const std::string &filePath, const std::string &actionName);
	void DrawSimulationScreenUI();
	void DrawSimulationSaveControls();
	// ゲーム画面の編集サイドバーとシミュレーション画面で共用するボス調整欄。
	void DrawBossBattleControls();
	// ボスの位置・スラスター位置を含む調整値を、次回のボス戦にも使えるJSONへ保存する。
	bool SaveBossThrusterSettings(const std::string& filePath = "resources/boss_thruster_settings.json");
	bool LoadBossThrusterSettings(const std::string& filePath = "resources/boss_thruster_settings.json", bool verbose = true);

	void DrawAnimationEditorUI();
	void RefreshActionAnimationsList();

	const std::set<std::string>& GetSelectedBoneNames() const { return selectedBoneNames_; }
	void AddSelectedBoneName(const std::string& name) { selectedBoneNames_.insert(name); }
	void RemoveSelectedBoneName(const std::string& name) { selectedBoneNames_.erase(name); }
	void ClearSelectedBones() { selectedBoneNames_.clear(); }
	bool IsBoneSelected(const std::string& name) const { return selectedBoneNames_.count(name) > 0; }
	
	void AddBoneRotationFromDrag(const std::string& boneName, float deltaX, float deltaY);
	void AddBoneTranslationFromDrag(const std::string& boneName, float deltaX, float deltaY);
	
	void UpdateShortcuts();
	void PushUndo();
	void Undo();
	void Redo();
	void DeleteSelectedBonesKeyframes();

private:
	GamePlayScene* scene_;

	// アニメーション編集用状態
	Animation customAnimation_;
	char actionName_[128] = "Guard";
	std::vector<std::string> availableActionAnimations_;
	
	std::set<std::string> selectedBoneNames_;
	float currentFrameTime_ = 0.0f;
	bool isAnimationPlaying_ = false;
	bool isLooping_ = true;

	std::vector<Animation> undoHistory_;
	std::vector<Animation> redoHistory_;
	bool isDragging_ = false; // ドラッグ状態の追跡
};

