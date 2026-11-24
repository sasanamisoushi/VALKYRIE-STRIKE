#pragma once

#include "engine/math/MyMath.h"

#include <list>
#include <memory>

class Camera;
class Enemy;
class FlyCamera;
class GamePlayUIManager;
class Obstacle;
class Player;
class SimulationManager;

// デバッグカメラの初期配置、ゲームビュー上のマウスによる敵配置、
// アニメーション編集時のボーン選択・ボックス選択を担当する開発用コントローラ。
class SceneDebugEditorController {
public:
	void ResetSelectionState();
	void ResetCameraToPlayer(FlyCamera* debugCamera, const Camera* fallbackCamera, const Player* player) const;
	void ResetCameraToStageOverview(
		FlyCamera* debugCamera,
		const Camera* fallbackCamera,
		const Player* player,
		const std::list<std::unique_ptr<Obstacle>>& obstacles) const;
	void TryPlaceEnemyAtGameViewMouse(
		FlyCamera* debugCamera,
		GamePlayUIManager* uiManager,
		SimulationManager* simulationManager) const;
	void UpdateAnimationEditorBoneSelection(
		FlyCamera* debugCamera,
		Player* player,
		SimulationManager* simulationManager);
	void UpdateBossEditorPartSelection(
		Camera* editorCamera,
		bool isEditorPreviewPlaying,
		GamePlayUIManager* uiManager,
		const std::list<std::unique_ptr<Enemy>>& enemies) const;

private:
	bool isBoxSelecting_ = false;
	Vector2 boxSelectStartPos_ = { 0.0f, 0.0f };
	Vector2 boxSelectEndPos_ = { 0.0f, 0.0f };
	bool clickedOnBone_ = false;
	bool isDraggingBone_ = false;
	Vector2 mouseDownPos_ = { 0.0f, 0.0f };
};
