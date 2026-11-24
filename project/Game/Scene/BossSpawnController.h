#pragma once

#include "Game/enemy/Boss.h"

#include <list>
#include <memory>

class Enemy;
class Obstacle;

// ボス本体を生成し、ステージ境界内へ安全に配置する。
class BossSpawnController {
public:
	void Spawn(const Vector3& requestedPosition, const Boss::Tuning* tuning,
		const std::list<std::unique_ptr<Obstacle>>& obstacles,
		std::list<std::unique_ptr<Enemy>>& enemies) const;
};
