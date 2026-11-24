#pragma once

class Enemy;
class MissilePresetManager;
class TitleBackgroundEnemyState;

// タイトル背景用の敵機が行うミサイル攻撃のテンポと弾種を管理する。
class TitleBackgroundEnemyWeaponController {
public:
	void Update(Enemy& target, TitleBackgroundEnemyState& state,
		MissilePresetManager* missilePresetManager) const;
};
