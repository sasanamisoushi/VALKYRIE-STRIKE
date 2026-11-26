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

void Player::Destroy() {
    isDead_ = true;
    isGuarding_ = false;
    SetPresentationThrustersVisible(false);
    // ゲームオーバーのシネマティック中は、爆発に包まれる機体を描画し続ける。
    // シーン遷移または再初期化でモデルは通常どおり破棄される。
}

void Player::TakeDamage(int damage) {
    if (isDead_ || isSpecialAttackActive_ || IsDodging() || IsGuarding()) return;
    hp_ -= damage;
    if (hp_ <= 0) {
        hp_ = 0;
        Destroy();
    }
}

void Player::SetSpecialAttackActive(bool active) {
    if (active && !isSpecialAttackActive_) {
        const Vector3 currentCameraForward = LengthSq(lastCameraDirection_) > 0.0001f
            ? NormalizeOrZero(lastCameraDirection_)
            : NormalizeOrZero(GetForwardVector());
        specialAttackCameraYaw_ = std::atan2(currentCameraForward.x, currentCameraForward.z);
        specialAttackCameraPitch_ = CameraPitchFromForward(currentCameraForward);
    }
    isSpecialAttackActive_ = active;
}

void Player::SetSongActive(bool active) {
    isSongActive_ = active;
}
