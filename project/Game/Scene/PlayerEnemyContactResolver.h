#pragma once

class Enemy;
class Player;

// プレイヤーと敵の接触によるめり込みだけを解消する。ダメージ判定は扱わない。
class PlayerEnemyContactResolver {
public:
	bool Resolve(Player& player, const Enemy& enemy) const;
};
