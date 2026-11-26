#pragma once

#include "engine/math/MyMath.h"
#include <list>
#include <memory>

class GroundEnemy;
class Obstacle;

// 地上敵の被弾リアクション・部位飛散・連鎖誘爆・被弾発光演出を担当する。
class GroundEnemyDestructionController {
public:
	void TakeDamage(GroundEnemy& enemy, int damage, const Vector3& hitDirection) const;
	void UpdateHitReaction(GroundEnemy& enemy, const std::list<std::unique_ptr<Obstacle>>& obstacles) const;
	void BeginDestruction(GroundEnemy& enemy, const Vector3& hitDirection) const;
	void UpdateDetachedParts(GroundEnemy& enemy) const;
	void UpdateDestruction(GroundEnemy& enemy) const;
	void UpdateDamageGlow(GroundEnemy& enemy) const;
};
