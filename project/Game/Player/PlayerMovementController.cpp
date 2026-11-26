#include "PlayerMovementController.h"

#include "Player.h"
#include "Game/base/GameSettings.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr float kBattroidGravity = 0.006f;
	constexpr float kBattroidMaxFallSpeed = 0.16f;

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

	Vector3 NormalizeOrZero(const Vector3& value) {
		const float length = Length(value);
		if (length <= 0.0001f) {
			return { 0.0f, 0.0f, 0.0f };
		}
		return Scale(value, 1.0f / length);
	}

	Vector3 ClampLength(const Vector3& value, float maxLength) {
		const float length = Length(value);
		if (length <= maxLength || length <= 0.0001f) {
			return value;
		}
		return Scale(value, maxLength / length);
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

	Quaternion MakeYawQuaternionFromForward(const Vector3& forward) {
		Vector3 flatForward = NormalizeOrZero({ forward.x, 0.0f, forward.z });
		if (LengthSq(flatForward) <= 0.0001f) {
			flatForward = { 0.0f, 0.0f, 1.0f };
		}

		const float yaw = std::atan2(flatForward.x, flatForward.z);
		return MyMath::Normalize(MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw));
	}

	float CameraPitchFromForward(const Vector3& forward) {
		Vector3 normalizedForward = NormalizeOrZero(forward);
		if (LengthSq(normalizedForward) <= 0.0001f) {
			return 0.0f;
		}
		return std::clamp(-std::asin(std::clamp(normalizedForward.y, -1.0f, 1.0f)), -1.2f, 1.2f);
	}
}

void PlayerMovementController::Update(Player& player, bool rotationLocked) const {
	auto& currentMode_ = player.currentMode_;
	auto& modeParams_ = player.modeParams_;
	auto& isGuarding_ = player.isGuarding_;
	auto& isBattroidWalking_ = player.isBattroidWalking_;
	auto& battroidWalkTime_ = player.battroidWalkTime_;
	auto& dodgeCooldownTimer_ = player.dodgeCooldownTimer_;
	auto& dodgeTimer_ = player.dodgeTimer_;
	auto& dodgeDirection_ = player.dodgeDirection_;
	auto& cameraPitch_ = player.cameraPitch_;
	auto& quaternion_ = player.quaternion_;
	auto& isSongActive_ = player.isSongActive_;
	auto& velocity_ = player.velocity_;
	auto& position_ = player.position_;
	auto& isMeleeAttacking_ = player.isMeleeAttacking_;
	auto& meleeTimer_ = player.meleeTimer_;
	const auto ChangeMode = [&player](PlayerMode mode) { player.ChangeMode(mode); };
	const auto GetForwardVector = [&player]() { return player.GetForwardVector(); };
	const auto PlayActionAnimation = [&player](const std::string& actionName) { player.PlayActionAnimation(actionName); };
	auto input = Input::GetInstance();

	if (input->TriggerAction(PlayerAction::TransformFighter)) ChangeMode(PlayerMode::Fighter);
	if (input->TriggerAction(PlayerAction::TransformGerwalk) ||
		input->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_LEFT)) ChangeMode(PlayerMode::Gerwalk);
	if (input->TriggerAction(PlayerAction::TransformBattroid)) ChangeMode(PlayerMode::Battroid);
	if (input->TriggerControllerButton(XINPUT_GAMEPAD_Y)) {
		PlayerMode nextMode = static_cast<PlayerMode>((static_cast<int>(currentMode_) + 1) % 3);
		ChangeMode(nextMode);
	}

	PlayerModeParams& p = modeParams_[static_cast<int>(currentMode_)];

	const bool moveForward = input->PushAction(PlayerAction::MoveForward);
	const bool moveBackward = input->PushAction(PlayerAction::MoveBackward);
	const bool moveUp = input->PushAction(PlayerAction::MoveUp);
	const bool moveDown = input->PushAction(PlayerAction::MoveDown);
	const bool guardInput = input->PushAction(PlayerAction::Guard);
	isGuarding_ = guardInput && p.canGuard && !player.IsSpecialAttackActive();
	isBattroidWalking_ = currentMode_ == PlayerMode::Battroid && (moveForward || moveBackward);
	if (!isBattroidWalking_) {
		battroidWalkTime_ = 0.0f;
	}

	const StickState leftStick = input->GetControllerLeftStick();
	const StickState rightStick = input->GetControllerRightStick();

	float pitch = 0.0f;
	float yaw = 0.0f;
	float roll = 0.0f;
	float cameraPitchDelta = 0.0f;
	const bool usesDetachedCameraPitch = currentMode_ != PlayerMode::Fighter;

	// 回避はファイター形態専用。TriggerKeyなので1入力につき1回だけ回転する。
	if (currentMode_ == PlayerMode::Fighter && dodgeCooldownTimer_ <= 0 && dodgeTimer_ <= 0) {
		if (input->TriggerAction(PlayerAction::DodgeLeft)) {
			dodgeTimer_ = Player::kDodgeDurationFrames;
			dodgeDirection_ = -1.0f;
		} else if (input->TriggerAction(PlayerAction::DodgeRight)) {
			dodgeTimer_ = Player::kDodgeDurationFrames;
			dodgeDirection_ = 1.0f;
		}
	}

	// 右スティック：カメラ操作・視点移動（全形態共通で視点・旋回操作）
	if (!rotationLocked) {
		if (std::abs(rightStick.x) > 0.0001f) {
			yaw += rightStick.x * p.yawSpeed * 1.5f;
		}
		if (std::abs(rightStick.y) > 0.0001f) {
			if (usesDetachedCameraPitch) {
				cameraPitchDelta -= rightStick.y * p.pitchSpeed * 1.5f;
			} else {
				pitch -= rightStick.y * p.pitchSpeed * 0.75f;
			}
		}
	}

	// 姿勢制御（共通）
	if (!rotationLocked && input->PushAction(PlayerAction::PitchUp)) {
		if (usesDetachedCameraPitch) {
			cameraPitchDelta -= p.pitchSpeed;
		} else {
			pitch -= p.pitchSpeed;
		}
	}
	if (!rotationLocked && input->PushAction(PlayerAction::PitchDown)) {
		if (usesDetachedCameraPitch) {
			cameraPitchDelta += p.pitchSpeed;
		} else {
			pitch += p.pitchSpeed;
		}
	}
	if (!rotationLocked && input->PushAction(PlayerAction::TurnRight)) yaw += p.yawSpeed;
	if (!rotationLocked && input->PushAction(PlayerAction::TurnLeft)) yaw -= p.yawSpeed;
	// ファイターでは A / D を機体の旋回入力としても使う。ガウォークでは同じ
	// 入力が横移動になるため、形態を切り替えた時に操作目的がはっきり変わる。
	if (!rotationLocked && currentMode_ == PlayerMode::Fighter) {
		if (input->PushAction(PlayerAction::DodgeLeft)) yaw -= p.yawSpeed;
		if (input->PushAction(PlayerAction::DodgeRight)) yaw += p.yawSpeed;
	}
	if (!usesDetachedCameraPitch) {
		if (!rotationLocked && input->PushAction(PlayerAction::RollRight)) roll -= p.rollSpeed;
		if (!rotationLocked && input->PushAction(PlayerAction::RollLeft)) roll += p.rollSpeed;
	}

	// 左スティック（ファイター時）：ピッチ（Y軸）およびロール（X軸）
	if (!rotationLocked && currentMode_ == PlayerMode::Fighter) {
		if (std::abs(leftStick.y) > 0.0001f) {
			pitch -= leftStick.y * p.pitchSpeed;
		}
		if (std::abs(leftStick.x) > 0.0001f) {
			roll -= leftStick.x * p.rollSpeed;
		}
	}

	// マウス入力による回転（視点・機体回転）
	long mouseDX = input->GetMouseDeltaX();
	long mouseDY = input->GetMouseDeltaY();
	const float mouseSensitivity = GameSettings::GetInstance().GetMouseSensitivity();
	if (!rotationLocked && mouseDX != 0) {
		yaw += mouseDX * mouseSensitivity;
	}
	if (!rotationLocked && mouseDY != 0) {
		if (usesDetachedCameraPitch) {
			cameraPitchDelta += mouseDY * mouseSensitivity;
		} else {
			pitch += mouseDY * mouseSensitivity;
		}
	}
	if (usesDetachedCameraPitch && std::abs(cameraPitchDelta) > 0.0001f) {
		cameraPitch_ = std::clamp(cameraPitch_ + cameraPitchDelta, -1.2f, 1.2f);
	}

	// --- ピッチ制限（上下180度制限：FPS視点風のジンバルロック） ---
	if (std::abs(pitch) > 0.0001f) {
		Quaternion qTestPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion testQuat = MyMath::Multiply(quaternion_, qTestPitch);
		Vector3 testForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, testQuat);
		// Y成分が±0.99f (約82度) を超えたらピッチ回転をキャンセルする
		if (testForward.y > 0.99f || testForward.y < -0.99f) {
			pitch = 0.0f;
		}
	}

	Vector3 localMove = { 0.0f, 0.0f, 0.0f };

	if (currentMode_ == PlayerMode::Fighter) {
		// ファイター形態：常に前進。RTで加速、LTで減速
		localMove.z += 0.5f; // 基本前進推力
		if (moveForward) localMove.z += 1.0f; // 加速 (RT)
		if (moveBackward) localMove.z -= 0.3f; // 減速 (LT)

	} else if (currentMode_ == PlayerMode::Gerwalk) {
		// ガウォーク形態：全方位へのカニ歩き、ホバリング
		// 左スティックで前後左右へのストレイフ移動
		if (std::abs(leftStick.x) > 0.0001f) localMove.x += leftStick.x;
		if (std::abs(leftStick.y) > 0.0001f) localMove.z += leftStick.y;
		if (moveForward) localMove.z += 1.0f; // RT: 前進
		if (moveBackward) localMove.z -= 1.0f; // LT: 後退
		if (input->PushAction(PlayerAction::DodgeLeft)) localMove.x -= 1.0f;
		if (input->PushAction(PlayerAction::DodgeRight)) localMove.x += 1.0f;
		if (moveUp) localMove.y += 1.0f;       // A: 上昇
		if (moveDown) localMove.y -= 1.0f;     // L3: 下降

	} else if (currentMode_ == PlayerMode::Battroid) {
		// バトロイド形態：全方位移動
		if (std::abs(leftStick.x) > 0.0001f) localMove.x += leftStick.x;
		if (std::abs(leftStick.y) > 0.0001f) localMove.z += leftStick.y;
		if (moveForward) localMove.z += 1.0f; // RT
		if (moveBackward) localMove.z -= 1.0f; // LT
		if (input->PushAction(PlayerAction::DodgeLeft)) localMove.x -= 1.0f;
		if (input->PushAction(PlayerAction::DodgeRight)) localMove.x += 1.0f;
		if (moveUp) localMove.y += 1.0f;       // A: 上昇
		if (moveDown) localMove.y -= 1.0f;     // L3: 下降
	}

	Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
	Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
	Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, roll);

	if (usesDetachedCameraPitch) {
		quaternion_ = MyMath::Multiply(quaternion_, qYaw);
		quaternion_ = MakeYawQuaternionFromForward(MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_));
	} else {
		quaternion_ = MyMath::Multiply(quaternion_, qPitch);
		quaternion_ = MyMath::Multiply(quaternion_, qYaw);
		quaternion_ = MyMath::Multiply(quaternion_, qRoll);
		quaternion_ = MyMath::Normalize(quaternion_);
	}

	// ======== 自動水平復元処理 ========
	bool isInputRotation = (std::abs(pitch) > 0.0001f || std::abs(yaw) > 0.0001f || std::abs(roll) > 0.0001f);
	if (currentMode_ == PlayerMode::Fighter && !isInputRotation && !rotationLocked) {
		Vector3 fwd = GetForwardVector();
		fwd.y = 0.0f; // Y成分を0にして水平にする
		if (LengthSq(fwd) > 0.0001f) {
			fwd = NormalizeOrZero(fwd);
			Vector3 defaultFwd = { 0.0f, 0.0f, 1.0f };
			float dot = MyMath::Dot(defaultFwd, fwd);
			dot = std::clamp(dot, -1.0f, 1.0f);

			Quaternion targetQ;
			if (dot > 0.999f) {
				targetQ = { 0.0f, 0.0f, 0.0f, 1.0f };
			} else if (dot < -0.999f) {
				targetQ = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, 3.14159265f);
			} else {
				Vector3 axis = MyMath::Cross(defaultFwd, fwd);
				if (LengthSq(axis) > 0.000001f) {
					axis = NormalizeOrZero(axis);
					targetQ = MyMath::MakeAxisAngle(axis, std::acos(dot));
				} else {
					targetQ = quaternion_; // 念のため
				}
			}
			// 現在の姿勢から水平な姿勢へ補間（1フレームで2%近づく）
			quaternion_ = MyMath::Slerp(quaternion_, targetQ, 0.02f);
			quaternion_ = MyMath::Normalize(quaternion_);
		}
	}
	// ==================================

	if (LengthSq(localMove) > 0.0001f) {
		localMove = NormalizeOrZero(localMove);
		Vector3 worldMove = MyMath::RotateVector(localMove, quaternion_);

		float currentMaxSpeed = p.maxMoveSpeed;
		float currentAccel = p.moveAcceleration;

		if (currentMode_ == PlayerMode::Fighter) {
			bool mForward = input->PushAction(PlayerAction::MoveForward);
			bool mBackward = input->PushAction(PlayerAction::MoveBackward);

			if (mForward) {
				currentMaxSpeed *= 3.0f;
				currentAccel *= 5.0f;
			} else if (mBackward) {
				currentMaxSpeed *= 0.4f;
			} else {
				currentMaxSpeed *= 0.8f;
			}
		}

		float finalAccel = isSongActive_ ? currentAccel * 1.5f : currentAccel;
		float finalMaxSpeed = isSongActive_ ? currentMaxSpeed * 1.5f : currentMaxSpeed;

		velocity_ = Add(velocity_, Scale(worldMove, finalAccel));

		if (LengthSq(velocity_) > finalMaxSpeed * finalMaxSpeed) {
			velocity_ = Scale(velocity_, 0.95f);
			if (LengthSq(velocity_) < finalMaxSpeed * finalMaxSpeed) {
				velocity_ = ClampLength(velocity_, finalMaxSpeed);
			}
		} else {
			velocity_ = ClampLength(velocity_, finalMaxSpeed);
		}
	} else {
		velocity_ = Scale(velocity_, p.moveDamping);
		if (LengthSq(velocity_) <= 0.00001f) {
			velocity_ = { 0.0f, 0.0f, 0.0f };
		}
	}
	if (currentMode_ == PlayerMode::Battroid && !moveUp) {
		velocity_.y = (std::max)(velocity_.y - kBattroidGravity, -kBattroidMaxFallSpeed);
	}

	position_ = Add(position_, velocity_);
	if (dodgeTimer_ > 0) {
		const Vector3 dodgeRight = MyMath::RotateVector({ 1.0f, 0.0f, 0.0f }, quaternion_);
		// 16フレームへ短縮しても、従来とほぼ同じ距離を一気に抜けられるようにする。
		position_ = Add(position_, Scale(dodgeRight, dodgeDirection_ * 0.56f));
	}

	// 近接攻撃（Vキー）
	if (p.canMelee && !isMeleeAttacking_ && input->TriggerAction(PlayerAction::Melee)) {
		isMeleeAttacking_ = true;
		meleeTimer_ = 30; // 30フレーム持続
		PlayActionAnimation("Melee"); // アニメーションがあれば再生
	}

	if (isMeleeAttacking_) {
		meleeTimer_--;
		if (meleeTimer_ <= 0) {
			isMeleeAttacking_ = false;
		} else {
			// 前方へダッシュする力を少し加える
			Vector3 forward = GetForwardVector();
			velocity_ = Add(velocity_, Scale(forward, 0.03f));
		}
	}

	if (isMeleeAttacking_) {
		// 近接攻撃中は他の移動入力を無視するか、弱める
		pitch = 0.0f;
		yaw = 0.0f;
		roll = 0.0f;
	} else if (dodgeTimer_ > 0) {
		dodgeTimer_--;
		if (dodgeTimer_ <= 0) {
			dodgeCooldownTimer_ = Player::kDodgeCooldownFrames;
		}
	} else if (dodgeCooldownTimer_ > 0) {
		--dodgeCooldownTimer_;
	}
}

void PlayerMovementController::UpdateLockOnRotation(Player& player, const Vector3& targetPos) const {
	Vector3 toTarget = {
		targetPos.x - player.position_.x,
		targetPos.y - player.position_.y,
		targetPos.z - player.position_.z,
	};
	if (LengthSq(toTarget) < 0.0001f) return;

	const Vector3 targetForward = NormalizeOrZero(toTarget);
	if (player.currentMode_ == PlayerMode::Fighter) {
		// カメラと同じ方向へ即時固定し、機体の正面側へカメラが回り込むのを防ぐ。
		player.quaternion_ = MakeNoRollLookQuaternion(targetForward);
	} else {
		// 分離ピッチを使う形態では、機体は水平旋回、上下方向はカメラピッチに保持する。
		player.quaternion_ = MakeYawQuaternionFromForward(targetForward);
		player.cameraPitch_ = CameraPitchFromForward(targetForward);
	}

	if (player.object_) {
		player.object_->SetQuaternionRotate(player.quaternion_);
		player.object_->Update();
	}
}
