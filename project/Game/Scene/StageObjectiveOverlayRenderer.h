#pragma once

#include <list>
#include <memory>

class Enemy;
class Player;

// ステージ目標と残り敵数を表示する HUD オーバーレイを描画する。
class StageObjectiveOverlayRenderer {
public:
	void Draw(bool enabled, const Player* player,
		const std::list<std::unique_ptr<Enemy>>& enemies, bool isBossObjective,
		float screenWidth, float screenHeight) const;
};
