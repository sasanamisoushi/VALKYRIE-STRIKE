#pragma once
#include "engine/math/MyMath.h"

class Enemy;

// 誘導弾のマルチロック対象を表す構造体
// 通常敵・ボス本体だけでなく、ボスの各砲台部位も個別にロック対象として保持できる。
struct MultiLockTarget {
	Enemy* enemy = nullptr;
	int turretIndex = -1; // -1: 敵本体, 0以上: ボス砲台インデックス
	Vector3 targetPoint{};
	bool hasTargetPoint = false;
};
