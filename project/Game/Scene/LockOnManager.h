#pragma once

#include <string>
#include <vector>
#include "engine/Camera/Camera.h"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/bullet/Missile.h"
#include "Game/Scene/MultiLockTarget.h"
#include "externals/json.hpp"

class GamePlayScene;

class LockOnManager {
public:
	LockOnManager(GamePlayScene* scene);
	void UpdateLockOn(Camera *activeCamera, bool shouldUpdateGame);
	void SwitchLockOnTarget(Camera *activeCamera);
	Enemy *FindLockOnTarget(Camera *activeCamera);
	bool IsLockedEnemyAlive() const;
	Enemy *FindAimAssistTarget(Camera *activeCamera);
	bool IsPlayerJammed(Camera *activeCamera) const;
	bool FindNextMultiLockTarget(Camera *activeCamera, MultiLockTarget &outTarget) const;
	bool IsFighterLockDanger() const { return isFighterLockDanger_; }
	void BeginMultiLock();
	void PruneMultiLockTargets();
	void UpdateMultiLock(Camera *activeCamera);
	void FireMultiLockMissiles();
	void CancelMultiLock();
	bool HasLockedBossTurret() const;
	Vector3 GetLockedTargetPosition() const;
	float GetLockedTargetRadius() const;
	int GetLockedBossTurretIndex() const;

private:
	bool ShouldKeepCurrentLock(Camera *activeCamera, bool &outTooClose);
	void SelectBossTurretForLock(Camera *activeCamera);
	bool IsFighterReacquireBlocked(const Enemy *enemy) const;
	void ReleaseCurrentLock(bool preventImmediateReacquire);

	GamePlayScene* scene_;
	Enemy *fighterRecentlyReleasedEnemy_ = nullptr;
	int fighterReacquireCooldownFrames_ = 0;
	bool isFighterLockDanger_ = false;
};
