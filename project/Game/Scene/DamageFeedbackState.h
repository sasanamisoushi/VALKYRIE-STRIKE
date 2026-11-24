#pragma once

// プレイヤーの被弾検知と、被弾時の画面エフェクト継続時間を管理する。
// 実際のポストエフェクト適用は GamePlayScene が担当する。
class DamageFeedbackState {
public:
	static constexpr int kDefaultEffectDurationFrames = 30;

	void Reset();
	void ObservePlayerHP(int currentHP);
	bool ConsumeEffectFrame();

	bool IsEffectActive() const { return effectTimer_ > 0; }
	int GetRemainingFrames() const { return effectTimer_; }

private:
	static constexpr int kUninitializedHP = -1;

	int previousPlayerHP_ = kUninitializedHP;
	int effectTimer_ = 0;
};
