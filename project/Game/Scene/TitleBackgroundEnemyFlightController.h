#pragma once

#include <list>
#include <memory>

class Camera;
class Enemy;
class Obstacle;
class Player;
class TitleBackgroundEnemyState;

// タイトル背景用の敵機について、巡航軌道、画面内への補正、進入演出を更新する。
class TitleBackgroundEnemyFlightController {
public:
	void Update(Player& player, Enemy& enemy, Camera& camera,
		const std::list<std::unique_ptr<Obstacle>>& obstacles,
		TitleBackgroundEnemyState& state, int titleBackgroundFrame) const;
};
