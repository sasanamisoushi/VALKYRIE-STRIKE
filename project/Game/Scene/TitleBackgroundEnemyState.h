#pragma once

// タイトル背景で表示する敵機の再出現、画面外からの進入、攻撃テンポを管理する。
// 敵の生成、移動、ミサイル発射はGamePlaySceneが担当する。
class TitleBackgroundEnemyState {
public:
	static constexpr int kEnterDurationFrames = 70;

	void Reset();
	void SetRespawnDelay(int frames);
	void AdvanceRespawnTimer();
	void BeginEntering();
	bool AdvanceEntering();
	void AdvanceCombatTimer();

	bool IsRespawnPending() const { return respawnTimer_ > 0; }
	bool IsEntering() const { return entering_; }
	int GetEnterFrame() const { return enterFrame_; }
	int GetSpawnSide() const { return spawnSide_; }
	int GetCombatTimer() const { return combatTimer_; }
	bool ShouldFire() const;
	bool IsFirstCombatShot() const { return combatTimer_ == 25; }
	bool IsEarlyCombatShot() const { return combatTimer_ <= 40; }

private:
	int respawnTimer_ = 0;
	bool entering_ = false;
	int enterFrame_ = 0;
	int spawnSide_ = 1;
	int combatTimer_ = 0;
};
