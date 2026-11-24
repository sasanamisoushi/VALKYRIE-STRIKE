#pragma once

// ボス出現済みかどうかと、出現警告演出の残りフレームをまとめて管理する。
class BossEncounterState {
public:
	static constexpr int kIntroDurationFrames = 120;

	void Reset();
	void MarkBossSpawned();
	void BeginIntro();
	void AdvanceIntro();

	bool IsBossSpawned() const { return bossSpawned_; }
	bool IsIntroActive() const { return introFrames_ > 0; }
	int GetIntroFrames() const { return introFrames_; }

private:
	bool bossSpawned_ = false;
	int introFrames_ = 0;
};
