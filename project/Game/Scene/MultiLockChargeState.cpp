#include "MultiLockChargeState.h"

void MultiLockChargeState::Reset() {
	CancelCharge();
	isMissileInputArmed_ = false;
}

void MultiLockChargeState::BeginCharge() {
	isCharging_ = true;
	chargeFrames_ = 0;
	targets_.clear();
}

void MultiLockChargeState::CancelCharge() {
	isCharging_ = false;
	chargeFrames_ = 0;
	targets_.clear();
}

void MultiLockChargeState::AddTarget(const MultiLockTarget& target, bool resetChargeFrames) {
	targets_.push_back(target);
	if (resetChargeFrames) {
		chargeFrames_ = 0;
	}
}

void MultiLockChargeState::AdvanceChargeFrame() {
	++chargeFrames_;
}

void MultiLockChargeState::RemoveTargetsByEnemy(const Enemy* enemy) {
	PruneTargetsIf([enemy](const MultiLockTarget& target) {
		return target.enemy == enemy;
	});
}

void MultiLockChargeState::ClearTargets() {
	targets_.clear();
}

bool MultiLockChargeState::HasTarget(const Enemy* enemy, int turretIndex) const {
	for (const auto& locked : targets_) {
		if (locked.enemy == enemy && locked.turretIndex == turretIndex) {
			return true;
		}
	}
	return false;
}

int MultiLockChargeState::CountTargetsForEnemy(const Enemy* enemy) const {
	int count = 0;
	for (const auto& target : targets_) {
		if (target.enemy == enemy) {
			++count;
		}
	}
	return count;
}

bool MultiLockChargeState::ShouldAcquireNextTarget(size_t maxTargets, int intervalFrames) const {
	if (targets_.size() >= maxTargets) {
		return false;
	}
	return targets_.empty() || chargeFrames_ >= intervalFrames;
}
