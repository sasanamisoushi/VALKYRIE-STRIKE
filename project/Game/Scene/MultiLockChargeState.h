#pragma once

#include "MultiLockTarget.h"

#include <algorithm>
#include <cstddef>
#include <vector>

class Enemy;

// 誘導ミサイルのマルチロック対象・チャージ時間・入力準備状態を管理する。
class MultiLockChargeState {
public:
	void Reset();
	void BeginCharge();
	void CancelCharge();

	void AddTarget(const MultiLockTarget& target, bool resetChargeFrames = false);
	void AdvanceChargeFrame();
	void RemoveTargetsByEnemy(const Enemy* enemy);
	void ClearTargets();

	template <typename Predicate>
	void PruneTargetsIf(Predicate&& shouldRemove) {
		targets_.erase(
			std::remove_if(targets_.begin(), targets_.end(), shouldRemove),
			targets_.end());
	}

	template <typename Callback>
	void ForEachTargetMutable(Callback&& callback) {
		for (auto& target : targets_) {
			callback(target);
		}
	}

	bool IsCharging() const { return isCharging_; }
	int GetChargeFrames() const { return chargeFrames_; }
	bool IsMissileInputArmed() const { return isMissileInputArmed_; }
	void SetMissileInputArmed(bool armed) { isMissileInputArmed_ = armed; }

	const std::vector<MultiLockTarget>& GetTargets() const { return targets_; }
	bool HasTargets() const { return !targets_.empty(); }
	size_t GetTargetCount() const { return targets_.size(); }
	bool HasTarget(const Enemy* enemy, int turretIndex) const;
	int CountTargetsForEnemy(const Enemy* enemy) const;
	bool ShouldAcquireNextTarget(size_t maxTargets, int intervalFrames) const;

private:
	std::vector<MultiLockTarget> targets_;
	bool isCharging_ = false;
	int chargeFrames_ = 0;
	// シーンを開く操作に使ったクリックを、初弾として扱わないための準備状態。
	bool isMissileInputArmed_ = false;
};
