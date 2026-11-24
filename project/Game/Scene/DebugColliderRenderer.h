#pragma once

#include "3D/Object3d.h"

#include <list>
#include <memory>

class Enemy;
class EnemyBulletManager;
class MissileManager;
class Obstacle;
class Player;

// デバッグコライダーのワイヤーフレーム生成に必要な読み取り専用データ。
struct DebugColliderContext {
	bool enabled = false;
	bool updateWireframes = false;
	bool drawAllDebugFrames = true;
	int currentSimulationTarget = 0;
	const Player* player = nullptr;
	const std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	const std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const Enemy* lockedEnemy = nullptr;
	const MissileManager* missileManager = nullptr;
	const EnemyBulletManager* enemyBulletManager = nullptr;
};

// プレイヤー・敵・障害物・弾の当たり判定ワイヤーフレームを構築・描画する。
class DebugColliderRenderer {
public:
	void Initialize();
	void Update(const DebugColliderContext& context);
	void Draw(bool enabled) const;

private:
	std::unique_ptr<Object3d> colliderLinesObject_;
};
