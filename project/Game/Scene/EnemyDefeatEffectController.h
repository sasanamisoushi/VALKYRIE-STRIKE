#pragma once

#include "3D/Object3d.h"
#include "engine/math/MyMath.h"

#include <memory>
#include <vector>

class Enemy;
class ParticleManager;

// 通常敵の破片飛散とホーミング撃破時の吸い込み消滅演出を管理する。
class EnemyDefeatEffectController {
public:
	void InitializeModel();
	void SpawnNormalEnemyDebris(Enemy& enemy, ParticleManager* particleManager);
	void SpawnHomingEnemyImplosion(Enemy& enemy);
	void Update(ParticleManager* particleManager);
	void Draw(const Vector4 frustumPlanes[6]) const;
	void Clear();

private:
	struct NormalEnemyDebrisPiece {
		std::unique_ptr<Object3d> object;
		Vector3 velocity{};
		Vector3 rotation{};
		Vector3 angularVelocity{};
		Vector3 baseScale{};
		int remainingFrames = 0;
	};

	struct HomingEnemyImplosion {
		std::unique_ptr<Object3d> object;
		Vector3 baseScale{};
		Vector3 rotation{};
		int remainingFrames = 0;
	};

	void UpdateNormalEnemyDebris();
	void UpdateHomingEnemyImplosions(ParticleManager* particleManager);

	std::vector<NormalEnemyDebrisPiece> normalEnemyDebris_;
	std::vector<HomingEnemyImplosion> homingEnemyImplosions_;
};
