#pragma once

#include "Game/Scene/MultiLockTarget.h"
#include "engine/math/MyMath.h"

class Camera;
struct LockOnSceneContext;

// マルチロックチャージ中のターゲット探索・生存監視・一斉発射を担当する。
class MultiLockController {
public:
	bool FindNextTarget(const LockOnSceneContext& context, Camera* activeCamera, MultiLockTarget& outTarget) const;
	void Begin(const LockOnSceneContext& context) const;
	void PruneTargets(const LockOnSceneContext& context) const;
	void Update(const LockOnSceneContext& context, Camera* activeCamera) const;
	void FireMissiles(
		const LockOnSceneContext& context,
		bool isLockedEnemyAlive,
		const Vector3& lockedTargetPosition) const;
	void Cancel(const LockOnSceneContext& context) const;
};
