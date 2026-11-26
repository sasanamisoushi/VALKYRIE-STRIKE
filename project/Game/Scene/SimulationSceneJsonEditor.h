#pragma once

#include "engine/math/MyMath.h"

#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

class Enemy;
class Obstacle;
class Player;
class StageReloadController;
struct EnemySpawnData;

struct SimulationSceneJsonContext {
	const Player* player = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	StageReloadController* stageReloadController = nullptr;
	std::string* statusMessage = nullptr;
	std::function<void()> reloadSceneJson;
};

// scene.json への現在配置の保存、敵スポーンの追加、初期スポーン順の入れ替えを担う。
class SimulationSceneJsonEditor {
public:
	static bool SaveCurrentLayout(const std::string& filePath, const SimulationSceneJsonContext& context);
	static bool AddEnemySpawn(
		const std::string& filePath,
		const std::string& name,
		const std::string& enemyType,
		const Vector3& position,
		const Vector3& rotation,
		const SimulationSceneJsonContext& context);
	static bool MoveEnemySpawn(
		const std::string& filePath,
		size_t sourceIndex,
		size_t targetIndex,
		const SimulationSceneJsonContext& context);
};
