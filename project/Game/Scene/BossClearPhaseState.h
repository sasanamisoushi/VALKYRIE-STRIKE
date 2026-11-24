#pragma once

enum class BossClearPhase {
	None,
	DefeatCinematic,
	FinalExplosion,
	Wreck,
	DebrisAftermath,
	MissionClear,
};

// ボス撃破演出のフェーズ遷移を管理する。
// 各フェーズの物理・カメラ・粒子処理はGamePlaySceneが担当する。
class BossClearPhaseState {
public:
	void Reset() { phase_ = BossClearPhase::None; }
	void Set(BossClearPhase phase) { phase_ = phase; }
	BossClearPhase Get() const { return phase_; }
	bool Is(BossClearPhase phase) const { return phase_ == phase; }
	bool IsActive() const { return phase_ != BossClearPhase::None; }

private:
	BossClearPhase phase_ = BossClearPhase::None;
};
