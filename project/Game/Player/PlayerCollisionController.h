#pragma once

#include "engine/math/MyMath.h"
#include <list>
#include <memory>

class Obstacle;
class Player;

// プレイヤーの当たり判定形状（OBB/半径）と地形・障害物・ステージ境界の押し戻し処理を担当する。
class PlayerCollisionController {
public:
	Vector3 GetWorldHalfExtents(const Player& player) const;
	float GetCollisionRadius(const Player& player) const;
	OBB GetOBB(const Player& player) const;
	void ResolveCollisions(Player& player, const std::list<std::unique_ptr<Obstacle>>& obstacles) const;
};
