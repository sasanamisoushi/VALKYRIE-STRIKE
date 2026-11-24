#pragma once

#include "Game/enemy/Enemy.h"

// ロックオン中の敵・ボス部位インデックス・照準アシスト対象を一元管理する。
// 敵の撃破や全消去時に必ず参照を無効化し、生ポインタのダングリングを防ぐ。
class TargetSelectionState {
public:
	Enemy* GetLockedEnemy() const { return lockedEnemy_; }
	int GetLockedBossTurretIndex() const { return lockedBossTurretIndex_; }
	Enemy* GetAimAssistEnemy() const { return aimAssistEnemy_; }

	bool HasLockedEnemy() const { return lockedEnemy_ != nullptr; }
	bool HasAliveLockedEnemy() const { return lockedEnemy_ != nullptr && !lockedEnemy_->IsDead(); }
	bool HasLockedBossTurret() const { return lockedBossTurretIndex_ >= 0; }

	void SetLockedTarget(Enemy* enemy, int bossTurretIndex = -1) {
		lockedEnemy_ = enemy;
		lockedBossTurretIndex_ = enemy ? bossTurretIndex : -1;
	}

	void SetLockedBossTurretIndex(int bossTurretIndex) {
		lockedBossTurretIndex_ = lockedEnemy_ ? bossTurretIndex : -1;
	}

	void ClearLockedTarget() {
		lockedEnemy_ = nullptr;
		lockedBossTurretIndex_ = -1;
	}

	void SetAimAssistEnemy(Enemy* enemy) {
		aimAssistEnemy_ = enemy;
	}

	void ClearAimAssist() {
		aimAssistEnemy_ = nullptr;
	}

	void ClearEnemy(const Enemy* enemy) {
		if (!enemy) {
			return;
		}
		if (lockedEnemy_ == enemy) {
			ClearLockedTarget();
		}
		if (aimAssistEnemy_ == enemy) {
			ClearAimAssist();
		}
	}

	void ClearAll() {
		ClearLockedTarget();
		ClearAimAssist();
	}

private:
	Enemy* lockedEnemy_ = nullptr;
	int lockedBossTurretIndex_ = -1;
	Enemy* aimAssistEnemy_ = nullptr;
};
