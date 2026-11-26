#pragma once

#include "engine/math/MyMath.h"

class Camera;
class Player;

// プレイヤー追従カメラの更新・ロックオン解除補間・照準方向計算を担当する。
class PlayerCameraController {
public:
	void UpdateCamera(Player& player, Camera* camera, const Vector3* targetPos, bool allowInput) const;
	Vector3 GetForwardVector(const Player& player) const;
	Vector3 GetAttackDirection(const Player& player) const;
	void SyncRotationToLastCameraDirection(Player& player) const;
};
