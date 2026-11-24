#pragma once

#include <functional>

class ExplosionManager;
class GroundEnemy;
class ParticleManager;

// 地上敵が発行した演出イベントを、爆発とパーティクルの実行処理へ変換する。
class GroundEnemyEffectProcessor {
public:
	void Process(GroundEnemy& enemy, ExplosionManager* explosionManager, ParticleManager* particleManager,
		const std::function<void(GroundEnemy&)>& onNormalShotDefeat,
		const std::function<void(GroundEnemy&)>& onHomingMissileDefeat) const;
};
