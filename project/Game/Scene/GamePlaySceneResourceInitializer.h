#pragma once

#include "3D/Animation.h"
#include "3D/Object3d.h"
#include "3D/Primitive.h"
#include "3D/Trail.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"

#include <list>
#include <memory>

class EnemyBulletManager;
class ExplosionManager;
class GameCameraManager;
class MissileManager;
class Obstacle;
class ParticleManager;
class Player;

struct GamePlaySceneDebugPrimitives {
	std::unique_ptr<Primitive>* sphere = nullptr;
	std::unique_ptr<Primitive>* box = nullptr;
	std::unique_ptr<Object3d>* animatedCube = nullptr;
	Animation* animationData = nullptr;
	Skeleton* skeleton = nullptr;
	std::unique_ptr<Object3d>* skeletonLinesObject = nullptr;
	std::unique_ptr<Trail>* missileTrail = nullptr;
	std::unique_ptr<Object3d>* trailObject = nullptr;
};

struct GamePlaySceneCombatManagers {
	std::unique_ptr<Player>* player = nullptr;
	std::unique_ptr<MissileManager>* missileManager = nullptr;
	std::unique_ptr<ExplosionManager>* explosionManager = nullptr;
	std::unique_ptr<EnemyBulletManager>* enemyBulletManager = nullptr;
	std::unique_ptr<GameCameraManager>* cameraManager = nullptr;
};

// GamePlayScene の初期化時に必要なモデル・パーティクル・デバッグオブジェクト・基本マネージャ生成を担う。
class GamePlaySceneResourceInitializer {
public:
	static void InitializeBaseModelsAndPrimitives(const GamePlaySceneDebugPrimitives& targets);
	static void InitializeParticleGroups(ParticleManager* particleManager,
		const char* songParticleGroup, const char* speedRushParticleGroup);
	static void InitializeDebugCamera(std::unique_ptr<FlyCamera>& debugFlyCamera, bool& isDebugCameraActive);
	static void InitializeCombatManagers(const GamePlaySceneCombatManagers& targets,
		const char* playerModelName, ParticleManager* particleManager);
	static void PrimeStageStart(Player* player, Camera* camera, std::list<std::unique_ptr<Obstacle>>& obstacles);
};
