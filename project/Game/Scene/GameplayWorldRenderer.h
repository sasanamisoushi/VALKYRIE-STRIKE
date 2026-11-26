#pragma once

#include <list>
#include <memory>
#include <vector>

class BoundaryShieldRenderer;
class Camera;
class DebugColliderRenderer;
class Enemy;
class EnemyBulletManager;
class EnemyDefeatEffectController;
class EnvironmentRenderer;
class ExplosionManager;
class MissileManager;
class Object3d;
class Obstacle;
class Player;
class SongFlightTrailRenderer;
class TutorialGoalMarker;
class WarpExitPresentationState;
struct BoundaryShieldSettings;

struct GameplayWorldDrawContext {
	Camera* renderCamera = nullptr;
	bool isEditorPreviewPlaying = false;
	bool hideEnemyBulletsForIntro = false;
	bool showPlane = false;
	bool showModel = false;
	bool showBones = false;
	bool showDebugColliders = false;
	bool isTutorialEnabled = false;
	int tutorialPhaseFrames = 0;
	bool isSongActive = false;
	int songFrame = 0;
	EnvironmentRenderer* environmentRenderer = nullptr;
	Player* player = nullptr;
	MissileManager* missileManager = nullptr;
	EnemyBulletManager* enemyBulletManager = nullptr;
	const std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const EnemyDefeatEffectController* enemyDefeatEffectController = nullptr;
	const std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	const std::vector<Object3d*>* objects = nullptr;
	Object3d* myModelObject = nullptr;
	TutorialGoalMarker* tutorialGoalMarker = nullptr;
	BoundaryShieldRenderer* boundaryShieldRenderer = nullptr;
	const BoundaryShieldSettings* boundaryShieldSettings = nullptr;
	Object3d* skeletonLinesObject = nullptr;
	DebugColliderRenderer* debugColliderRenderer = nullptr;
	WarpExitPresentationState* warpExitPresentation = nullptr;
	SongFlightTrailRenderer* songFlightTrailRenderer = nullptr;
	ExplosionManager* explosionManager = nullptr;
};

// 3D ワールドの不透明パス・半透明パス・デバッグ線分・加算エフェクトパスの描画順を管理する。
class GameplayWorldRenderer {
public:
	static void Draw(const GameplayWorldDrawContext& context);
};
