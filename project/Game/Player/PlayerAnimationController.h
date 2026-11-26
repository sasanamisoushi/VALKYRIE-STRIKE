#pragma once

class Player;

// プレイヤーの変形・アクションアニメーション再生、歩行/ガードポーズ適用、およびモデル姿勢更新を担当する。
class PlayerAnimationController {
public:
	void UpdateModel(Player& player, bool allowInput, bool advanceState) const;
	void ApplyBattroidProceduralWalk(Player& player) const;
	void ApplyGuardPose(Player& player, float blendWeight) const;
};
