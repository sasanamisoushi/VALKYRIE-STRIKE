#pragma once

#include <list>
#include <memory>

class Enemy;
class Player;
class TitleBackgroundEnemyState;

// タイトル背景用の敵機を選択し、全滅時の再出現待機と生成を担当する。
// 進入中の移動・射撃テンポはTitleBackgroundEnemyStateとGamePlaySceneが担当する。
class TitleBackgroundEnemyController {
public:
	Enemy* AcquireTarget(Player& player, std::list<std::unique_ptr<Enemy>>& enemies,
		TitleBackgroundEnemyState& state) const;
};
