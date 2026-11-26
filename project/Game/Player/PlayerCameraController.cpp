#include "PlayerCameraController.h"

#include "Player.h"
#include "engine/Camera/Camera.h"
#include "engine/Input/Input.h"

#include <algorithm>
#include <cmath>

namespace {
	float LengthSq(const Vector3& value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float Length(const Vector3& value) {
		return std::sqrt(LengthSq(value));
	}

	Vector3 Add(const Vector3& a, const Vector3& b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vector3 Scale(const Vector3& value, float scalar) {
		return { value.x * scalar, value.y * scalar, value.z * scalar };
	}

	Vector3 Lerp(const Vector3& from, const Vector3& to, float t) {
		return {
			from.x + (to.x - from.x) * t,
			from.y + (to.y - from.y) * t,
			from.z + (to.z - from.z) * t,
		};
	}

	Vector3 NormalizeOrZero(const Vector3& value) {
		const float length = Length(value);
		if (length <= 0.0001f) {
			return { 0.0f, 0.0f, 0.0f };
		}
		return Scale(value, 1.0f / length);
	}

	Quaternion MakeNoRollLookQuaternion(const Vector3& forward) {
		Vector3 normalizedForward = NormalizeOrZero(forward);
		if (LengthSq(normalizedForward) <= 0.0001f) {
			normalizedForward = { 0.0f, 0.0f, 1.0f };
		}

		const float clampedY = std::clamp(normalizedForward.y, -1.0f, 1.0f);
		const float pitch = -std::asin(clampedY);
		const float yaw = std::atan2(normalizedForward.x, normalizedForward.z);
		Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
		return MyMath::Normalize(MyMath::Multiply(qYaw, qPitch));
	}

	Vector3 MakeForwardFromFlatAndPitch(const Vector3& flatForward, float pitch) {
		Vector3 normalizedFlat = NormalizeOrZero({ flatForward.x, 0.0f, flatForward.z });
		if (LengthSq(normalizedFlat) <= 0.0001f) {
			normalizedFlat = { 0.0f, 0.0f, 1.0f };
		}

		const float yaw = std::atan2(normalizedFlat.x, normalizedFlat.z);
		Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
		return MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, MyMath::Normalize(MyMath::Multiply(qYaw, qPitch)));
	}
}

void PlayerCameraController::UpdateCamera(Player& player, Camera* camera, const Vector3* targetPos, bool allowInput) const {
	Vector3 playerForward = NormalizeOrZero(GetForwardVector(player));
	if (LengthSq(playerForward) <= 0.0001f) {
		playerForward = { 0.0f, 0.0f, 1.0f };
	}
	Vector3 flatForward = NormalizeOrZero({ playerForward.x, 0.0f, playerForward.z });
	if (LengthSq(flatForward) <= 0.0001f) {
		flatForward = { 0.0f, 0.0f, 1.0f };
	}

	Vector3 cameraDirection;
	Vector3 lookTarget = player.GetOBB().center;

	if (player.isSpecialAttackActive_) {
		cameraDirection = GetAttackDirection(player);
	} else if (targetPos) {
		Vector3 toTarget = { targetPos->x - lookTarget.x, targetPos->y - lookTarget.y, targetPos->z - lookTarget.z };
		Vector3 flatToTarget = { toTarget.x, 0.0f, toTarget.z };
		float distToTargetSq = LengthSq(flatToTarget);

		Vector3 targetDir;
		if (distToTargetSq > 0.0001f) {
			targetDir = NormalizeOrZero(flatToTarget);
		} else {
			targetDir = flatForward;
		}

		cameraDirection = targetDir;
		// ロック対象との高低差で視点が急角度にならないよう、カメラ台の傾きは抑える。
		float pitchBlend = std::clamp(toTarget.y / 50.0f, -0.28f, 0.28f);
		cameraDirection.y = pitchBlend;
		cameraDirection = NormalizeOrZero(cameraDirection);

		float t = 0.35f;
		Vector3 blendTarget = {
			lookTarget.x + toTarget.x * t,
			lookTarget.y + toTarget.y * t,
			lookTarget.z + toTarget.z * t
		};

		float maxLookAhead = 15.0f;
		Vector3 toBlend = { blendTarget.x - lookTarget.x, blendTarget.y - lookTarget.y, blendTarget.z - lookTarget.z };
		if (LengthSq(toBlend) > maxLookAhead * maxLookAhead) {
			toBlend = NormalizeOrZero(toBlend);
			lookTarget = {
				lookTarget.x + toBlend.x * maxLookAhead,
				lookTarget.y + toBlend.y * maxLookAhead,
				lookTarget.z + toBlend.z * maxLookAhead
			};
		} else {
			lookTarget = blendTarget;
		}
	} else {
		Vector3 rawDirection = {
			playerForward.x,
			// 機体のピッチをそのまま視点に反映すると、急上昇・急降下で
			// 地平線が大きく動いて酔いやすくなるため、追従角を制限する。
			std::clamp(playerForward.y, -0.28f, 0.28f),
			playerForward.z
		};
		if (LengthSq({ rawDirection.x, 0.0f, rawDirection.z }) <= 0.0001f) {
			rawDirection.x = flatForward.x * 0.35f;
			rawDirection.z = flatForward.z * 0.35f;
		}
		cameraDirection = NormalizeOrZero(rawDirection);
		if (LengthSq(cameraDirection) <= 0.0001f) {
			cameraDirection = flatForward;
		}
	}

	// 敵をロックしていた視線を解除と同時に自機の正面へ戻すと、注視点とカメラ位置が
	// 同フレームに大きく飛んでしまう。最後のロックオン視線を短時間だけ残して自然に戻す。
	const bool isNormalLockOnCamera = targetPos && !player.isSpecialAttackActive_;
	if (isNormalLockOnCamera) {
		player.wasCameraLockOnActive_ = true;
		player.cameraLockReleaseProgress_ = 1.0f;
		player.cameraLockReleaseDirection_ = cameraDirection;
		const Vector3 playerCenter = player.GetOBB().center;
		player.cameraLockReleaseFocusOffset_ = {
			lookTarget.x - playerCenter.x,
			lookTarget.y - playerCenter.y,
			lookTarget.z - playerCenter.z,
		};
	} else if (player.wasCameraLockOnActive_ && !player.isSpecialAttackActive_) {
		player.wasCameraLockOnActive_ = false;
		player.cameraLockReleaseProgress_ = 0.0f;
	}

	if (!player.isSpecialAttackActive_ && player.cameraLockReleaseProgress_ < 1.0f) {
		constexpr float kLockReleaseCameraBlendSpeed = 0.14f;
		player.cameraLockReleaseProgress_ = (std::min)(
			1.0f, player.cameraLockReleaseProgress_ + kLockReleaseCameraBlendSpeed);
		const float t = player.cameraLockReleaseProgress_ * player.cameraLockReleaseProgress_ *
			(3.0f - 2.0f * player.cameraLockReleaseProgress_);

		cameraDirection = NormalizeOrZero(Add(
			Scale(player.cameraLockReleaseDirection_, 1.0f - t),
			Scale(cameraDirection, t)));
		if (LengthSq(cameraDirection) <= 0.0001f) {
			cameraDirection = flatForward;
		}
		lookTarget = Add(player.GetOBB().center, Scale(player.cameraLockReleaseFocusOffset_, 1.0f - t));
	}

	// 必殺技中を含め、カメラ位置に使う上下方向を常に穏やかに保つ。
	// 注視点自体は維持するため、敵を画面外へ逃がさずに地平線の揺れだけを減らす。
	cameraDirection.y = std::clamp(cameraDirection.y, -0.28f, 0.28f);
	cameraDirection = NormalizeOrZero(cameraDirection);
	if (LengthSq(cameraDirection) <= 0.0001f) {
		cameraDirection = flatForward;
	}

	player.lastCameraDirection_ = cameraDirection;

	float backDistance = 12.0f;
	const float heightOffset = 2.5f;
	float speed = Length(player.velocity_);

	if (player.currentMode_ == PlayerMode::Fighter) {
		backDistance = 12.0f + (speed * 1.5f);
	}

	// 狭すぎる画角は高速移動時に周辺視野が失われ、酔いにつながりやすい。
	float targetFov = 0.60f;
	if (player.currentMode_ == PlayerMode::Fighter) {
		targetFov = 0.60f + (speed * 0.15f);
		targetFov = std::clamp(targetFov, 0.60f, 0.78f);
	}
	float currentFov = camera->GetFovY();
	camera->SetFovY(currentFov + (targetFov - currentFov) * 0.1f);

	Vector3 cameraBackOffset = Scale(cameraDirection, -backDistance);
	Vector3 playerOBBCenter = player.GetOBB().center;
	const Vector3 desiredCameraPosition = {
		playerOBBCenter.x + cameraBackOffset.x,
		playerOBBCenter.y + heightOffset + cameraBackOffset.y,
		playerOBBCenter.z + cameraBackOffset.z
	};

	// 通常時は少し遅れて追従させ、急な進行方向の変更による画面の跳ねを抑える。
	// ロックオン中は敵を見失わないよう、通常より速く目標位置へ寄せる。
	constexpr float kNormalCameraFollowBlend = 0.16f;
	constexpr float kLockOnCameraFollowBlend = 0.24f;
	const float followBlend = isNormalLockOnCamera ? kLockOnCameraFollowBlend : kNormalCameraFollowBlend;
	if (!player.isCameraFollowInitialized_) {
		player.cameraFollowPosition_ = desiredCameraPosition;
		player.isCameraFollowInitialized_ = true;
	} else {
		player.cameraFollowPosition_ = Lerp(player.cameraFollowPosition_, desiredCameraPosition, followBlend);
	}
	const Vector3 camPos = player.cameraFollowPosition_;
	Vector3 lookForward = NormalizeOrZero({
		lookTarget.x - camPos.x,
		lookTarget.y - camPos.y,
		lookTarget.z - camPos.z
	});
	if (LengthSq(lookForward) <= 0.0001f) {
		lookForward = flatForward;
	}
	Quaternion cameraRotation = MakeNoRollLookQuaternion(lookForward);

	if (player.currentMode_ == PlayerMode::Fighter) {
		static float currentCameraRoll = 0.0f;
		if (!allowInput) {
			// タイトル背景などではA/Dをカメラのバンクへ渡さない。
			currentCameraRoll = 0.0f;
		} else {
			auto input = Input::GetInstance();
			float rollAngle = 0.0f;
			if (input->PushAction(PlayerAction::DodgeLeft) || input->PushAction(PlayerAction::TurnLeft)) {
				rollAngle = 0.08f;
			} else if (input->PushAction(PlayerAction::DodgeRight) || input->PushAction(PlayerAction::TurnRight)) {
				rollAngle = -0.08f;
			}
			// バンクは方向転換の手掛かりとして残しつつ、視界の回転を最小限にする。
			currentCameraRoll = currentCameraRoll + (rollAngle - currentCameraRoll) * 0.06f;
		}

		Quaternion rollQ = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, currentCameraRoll);
		cameraRotation = MyMath::Multiply(cameraRotation, rollQ);
	}

	camera->SetTranslate(camPos);
	camera->SetRotate({ 0, 0, 0 });
	camera->SetQuaternion(cameraRotation);
}

Vector3 PlayerCameraController::GetForwardVector(const Player& player) const {
	Vector3 bodyForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, player.quaternion_);
	if (player.currentMode_ == PlayerMode::Fighter) {
		return bodyForward;
	}

	return MakeForwardFromFlatAndPitch(bodyForward, player.cameraPitch_);
}

Vector3 PlayerCameraController::GetAttackDirection(const Player& player) const {
	if (!player.isSpecialAttackActive_) {
		return GetForwardVector(player);
	}

	const Vector3 flatAimForward = {
		std::sin(player.specialAttackCameraYaw_),
		0.0f,
		std::cos(player.specialAttackCameraYaw_)
	};
	return MakeForwardFromFlatAndPitch(flatAimForward, player.specialAttackCameraPitch_);
}

void PlayerCameraController::SyncRotationToLastCameraDirection(Player& player) const {
	if (LengthSq(player.lastCameraDirection_) > 0.0001f) {
		float pitch = -std::asin(std::clamp(player.lastCameraDirection_.y, -1.0f, 1.0f));
		float yaw = std::atan2(player.lastCameraDirection_.x, player.lastCameraDirection_.z);
		// Update pitch and yaw based on last camera direction (keep roll as 0 to snap flat)
		player.SetRotation({ pitch, yaw, 0.0f });
	}
}
