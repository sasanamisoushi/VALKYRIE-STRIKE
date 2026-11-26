#pragma once

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Enemy;
class Obstacle;
class SimulationManager;
struct EnemySpawnData;

struct StagePlacementPanelContext {
	char* blenderEnemyName = nullptr;
	size_t blenderEnemyNameSize = 0;
	int* blenderEnemyTypeIndex = nullptr;
	float* blenderEnemyPosition = nullptr;        // float[3]
	float* blenderEnemyRotationDegrees = nullptr; // float[3]
	bool* enemyMousePlacementEnabled = nullptr;
	float* enemyMousePlacementHeight = nullptr;
	const std::string* simulationSaveMessage = nullptr;
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	SimulationManager* simulationManager = nullptr;
	std::function<void(bool)> setDebugCameraActive;
};

// ステージ配置（敵・障害物）・Blender起動・レベル検査表示・配布ZIP生成のUIを担う。
class StageEditorPanel {
public:
	void DrawEnemyAndObstaclePanel(const StagePlacementPanelContext& context) const;
	void DrawBlenderAndPreviewPanel(bool& isEditorPreviewPlaying,
		const std::function<void()>& resetEditorPreview);
	void DrawPackageGeneratorPanel();
	void DrawStageValidationOverlays(Camera* activeCamera) const;

private:
	std::string blenderLaunchMessage_;
	bool showStageValidationWindow_ = true;
	bool showStageValidationLabels_ = true;
};
