#include "PlayerActionController.h"
#include "Player.h"

#include <algorithm>
#include <cmath>

namespace {
float LengthSq(const Vector3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

Vector3 NormalizeOrZero(const Vector3& value) {
    const float lengthSq = LengthSq(value);
    if (lengthSq <= 0.0001f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSq);
    return { value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
}

float CameraPitchFromForward(const Vector3& forward) {
    const Vector3 normalizedForward = NormalizeOrZero(forward);
    if (LengthSq(normalizedForward) <= 0.0001f) {
        return 0.0f;
    }
    return std::clamp(-std::asin(std::clamp(normalizedForward.y, -1.0f, 1.0f)), -1.2f, 1.2f);
}
}

void PlayerActionController::Destroy(Player& player) const {
    player.isDead_ = true;
    player.isGuarding_ = false;
    player.presentationThrustersVisible_ = false;
    // ゲームオーバーのシネマティック中は、爆発に包まれる機体を描画し続ける。
    // シーン遷移または再初期化でモデルは通常どおり破棄される。
}

void PlayerActionController::TakeDamage(Player& player, int damage) const {
    if (player.isDead_ || player.isSpecialAttackActive_ || player.IsDodging() || player.IsGuarding()) return;
    player.hp_ -= damage;
    if (player.hp_ <= 0) {
        player.hp_ = 0;
        Destroy(player);
    }
}

void PlayerActionController::SetSpecialAttackActive(Player& player, bool active) const {
    if (active && !player.isSpecialAttackActive_) {
        const Vector3 currentCameraForward = LengthSq(player.lastCameraDirection_) > 0.0001f
            ? NormalizeOrZero(player.lastCameraDirection_)
            : NormalizeOrZero(player.GetForwardVector());
        player.specialAttackCameraYaw_ = std::atan2(currentCameraForward.x, currentCameraForward.z);
        player.specialAttackCameraPitch_ = CameraPitchFromForward(currentCameraForward);
    }
    player.isSpecialAttackActive_ = active;
}

void PlayerActionController::SetSongActive(Player& player, bool active) const {
    player.isSongActive_ = active;
}
