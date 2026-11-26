#pragma once

#include "engine/math/MyMath.h"
#include <cstddef>

class Boss;
class EnemyBulletManager;

// ボス砲台群（艦首砲・主砲・ミニ連装砲・機銃）の照準・モデル姿勢更新・射撃を担当する。
class BossTurretController {
public:
	void FireMachineGun(const Boss& boss, const Vector3& playerPos, EnemyBulletManager* bulletManager) const;
	void FireMainBeam(const Boss& boss, const Vector3& playerPos, EnemyBulletManager* bulletManager, size_t turretIndex) const;
	void FireForwardTurret(const Boss& boss, const Vector3& playerPos, EnemyBulletManager* bulletManager, size_t turretIndex) const;
	Vector3 GetTurretPivot(size_t turretIndex) const;
	float GetTurretHitRadius(size_t turretIndex) const;
	void UpdateAim(Boss& boss, const Vector3& playerPos) const;
	void UpdateModels(Boss& boss) const;
};
