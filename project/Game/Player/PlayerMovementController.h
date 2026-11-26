#pragma once

#include "engine/math/MyMath.h"

class Player;

// プレイヤーの入力解釈、姿勢制御、移動、回避、近接攻撃、ロックオン旋回を担当する。
// 描画・衝突・カメラ・アニメーションの更新は Player 側の別責務として残す。
class PlayerMovementController {
public:
    void Update(Player& player, bool rotationLocked) const;
    void UpdateLockOnRotation(Player& player, const Vector3& targetPos) const;
};
