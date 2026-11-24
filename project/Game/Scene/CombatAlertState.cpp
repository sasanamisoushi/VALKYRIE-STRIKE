#include "CombatAlertState.h"

void CombatAlertState::Reset() {
	enemyAttackFrames_ = 0;
	enemyAttackSource_ = {};
	reloadWarningFrames_ = 0;
	isHomingReloadWarning_ = false;
}

void CombatAlertState::Update() {
	if (enemyAttackFrames_ > 0) {
		--enemyAttackFrames_;
	}
	if (reloadWarningFrames_ > 0) {
		--reloadWarningFrames_;
	}
}

void CombatAlertState::TriggerEnemyAttack(const Vector3& source) {
	enemyAttackFrames_ = kEnemyAttackDurationFrames;
	enemyAttackSource_ = source;
}

void CombatAlertState::TriggerReloadWarning(bool isHomingWeapon) {
	reloadWarningFrames_ = kReloadWarningDurationFrames;
	isHomingReloadWarning_ = isHomingWeapon;
}
