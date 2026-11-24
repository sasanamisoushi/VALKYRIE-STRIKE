#pragma once

#include <list>
#include <memory>

class Enemy;

// 生存中の通常敵が重ならないよう、水平方向の位置補正を行う。
class EnemySeparationResolver {
public:
	void Resolve(std::list<std::unique_ptr<Enemy>>& enemies) const;
};
