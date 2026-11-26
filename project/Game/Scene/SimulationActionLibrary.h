#pragma once

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Enemy;
class EnemyBulletManager;
class EnemySpawnManager;
class ExplosionManager;
class FlyCamera;
class GameOverSequence;
class LockOnManager;
class MissileManager;
class Obstacle;
class ParticleManager;
class Player;
class TargetSelectionState;
struct EnemySpawnData;

struct SimulationActionMissileParams {
	float* normalSpeed = nullptr;
	float* normalScale = nullptr;
	float* normalCollisionRadius = nullptr;
	int* normalLifeTime = nullptr;
	float* speed = nullptr;
	float* ampX = nullptr;
	float* ampZ = nullptr;
	float* ampY = nullptr;
	float* freqY = nullptr;
	float* baseY = nullptr;
	float* homingStrength = nullptr;
	float* homingScale = nullptr;
	float* homingCollisionRadius = nullptr;
	float* trailWidth = nullptr;
	int* lifeTime = nullptr;
	float* muzzleOffset = nullptr;
};

struct SimulationActionContext {
	bool isDebugCameraActive = false;
	Camera* camera = nullptr;
	FlyCamera* debugFlyCamera = nullptr;
	Player* player = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	EnemySpawnManager* enemySpawnManager = nullptr;
	MissileManager* missileManager = nullptr;
	EnemyBulletManager* enemyBulletManager = nullptr;
	ExplosionManager* explosionManager = nullptr;
	ParticleManager* particleManager = nullptr;
	LockOnManager* lockOnManager = nullptr;
	TargetSelectionState* targetSelection = nullptr;
	GameOverSequence* gameOverSequence = nullptr;
	SimulationActionMissileParams missileParams{};
	std::vector<std::string>* actionNames = nullptr;
	int* selectedActionIndex = nullptr;
	std::string* statusMessage = nullptr;
	std::function<void()> resetCinematicLockOnCamera;
};

// 名前付きシミュレーション行動プリセットの保存・一覧更新・適用を担う。
class SimulationActionLibrary {
public:
	static void RefreshActionNames(const std::string& filePath,
		std::vector<std::string>& actionNames, int& selectedIndex);
	static bool SaveNamedAction(const std::string& filePath, const std::string& actionName,
		const SimulationActionContext& context);
	static bool ApplyAction(const std::string& filePath, const std::string& actionName,
		const SimulationActionContext& context);
};
