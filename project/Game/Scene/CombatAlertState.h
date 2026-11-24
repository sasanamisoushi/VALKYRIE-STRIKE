#pragma once

#include "engine/math/MyMath.h"

// 戦闘中に表示する一時的な警告の状態を管理する。
// 実際のHUD描画はGamePlaySceneが担当する。
class CombatAlertState {
public:
	static constexpr int kEnemyAttackDurationFrames = 90;
	static constexpr int kReloadWarningDurationFrames = 72;

	void Reset();
	void Update();
	void TriggerEnemyAttack(const Vector3& source);
	void TriggerReloadWarning(bool isHomingWeapon);

	int GetEnemyAttackFrames() const { return enemyAttackFrames_; }
	const Vector3& GetEnemyAttackSource() const { return enemyAttackSource_; }
	int GetReloadWarningFrames() const { return reloadWarningFrames_; }
	bool IsHomingReloadWarning() const { return isHomingReloadWarning_; }

private:
	int enemyAttackFrames_ = 0;
	Vector3 enemyAttackSource_{};
	int reloadWarningFrames_ = 0;
	bool isHomingReloadWarning_ = false;
};
