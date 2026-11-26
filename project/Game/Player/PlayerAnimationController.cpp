#include "Player.h"
#include "engine/Input/Input.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>

namespace {
	constexpr const char* kDefaultPlayerBoxModelName = "PlayerBox";
	constexpr float kBattroidIdleAnimationTime = 5.0f;
	constexpr float kBattroidWalkFrequency = 4.5f;
	constexpr float kBattroidFootSwingAngle = 0.14f;
	constexpr float kBattroidFootStepDistance = 0.18f;
	constexpr float kBattroidFootLiftDistance = 0.08f;

	bool UsesNaturalPlayerModelScale(const std::string& modelName) {
		return modelName != kDefaultPlayerBoxModelName;
	}

	float LengthSq(const Vector3& value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float Length(const Vector3& value) {
		return std::sqrt(LengthSq(value));
	}

	Vector3 Add(const Vector3& a, const Vector3& b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	std::string StripNodeIndexSuffix(const std::string& name) {
		const size_t underscore = name.find_last_of('_');
		if (underscore == std::string::npos || underscore + 1 >= name.size()) {
			return name;
		}

		for (size_t index = underscore + 1; index < name.size(); ++index) {
			if (name[index] < '0' || name[index] > '9') {
				return name;
			}
		}
		return name.substr(0, underscore);
	}

	int32_t FindJointByBaseName(const Skeleton& skeleton, const std::string& baseName) {
		for (const Joint& joint : skeleton.joints) {
			if (StripNodeIndexSuffix(joint.name) == baseName) {
				return joint.index;
			}
		}
		return -1;
	}

	void AddJointLocalRotation(Skeleton& skeleton, const std::string& baseName, const Vector3& axis, float angle) {
		const int32_t jointIndex = FindJointByBaseName(skeleton, baseName);
		if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
			return;
		}

		Quaternion extraRotation = MyMath::MakeAxisAngle(axis, angle);
		Joint& joint = skeleton.joints[jointIndex];
		joint.transform.rotate = MyMath::Normalize(MyMath::Multiply(joint.transform.rotate, extraRotation));
	}

	void AddJointLocalTranslation(Skeleton& skeleton, const std::string& baseName, const Vector3& translation) {
		const int32_t jointIndex = FindJointByBaseName(skeleton, baseName);
		if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
			return;
		}

		Joint& joint = skeleton.joints[jointIndex];
		joint.transform.translate = Add(joint.transform.translate, translation);
	}

	int32_t FindJointByExactName(const Skeleton& skeleton, const char* name) {
		for (const Joint& joint : skeleton.joints) {
			if (joint.name == name) {
				return joint.index;
			}
		}
		return -1;
	}

	Vector3 GetJointTranslation(const Skeleton& skeleton, const char* name) {
		const int32_t jointIndex = FindJointByExactName(skeleton, name);
		if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
			return { 0.0f, 0.0f, 0.0f };
		}
		return skeleton.joints[jointIndex].transform.translate;
	}

	// vf-15c は手足の装甲が別メッシュの剛体モデルなので、関係するノードを
	// 同じ支点で回転させて、ボーン親子付けと同じ追従を作る。
	void RotateJointGroupAroundPivot(
		Skeleton& skeleton,
		std::initializer_list<const char*> jointNames,
		const Vector3& pivot,
		const Vector3& axis,
		float angle) {
		if (std::abs(angle) <= 0.0001f) {
			return;
		}

		const Quaternion rotation = MyMath::MakeAxisAngle(axis, angle);
		for (const char* name : jointNames) {
			const int32_t jointIndex = FindJointByExactName(skeleton, name);
			if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
				continue;
			}

			Joint& joint = skeleton.joints[jointIndex];
			const Vector3 offset = {
				joint.transform.translate.x - pivot.x,
				joint.transform.translate.y - pivot.y,
				joint.transform.translate.z - pivot.z,
			};
			joint.transform.translate = Add(pivot, MyMath::RotateVector(offset, rotation));
			joint.transform.rotate = MyMath::Normalize(MyMath::Multiply(rotation, joint.transform.rotate));
		}
	}

	void TranslateJointGroup(Skeleton& skeleton, std::initializer_list<const char*> jointNames, const Vector3& translation) {
		for (const char* name : jointNames) {
			const int32_t jointIndex = FindJointByExactName(skeleton, name);
			if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
				continue;
			}
			skeleton.joints[jointIndex].transform.translate = Add(skeleton.joints[jointIndex].transform.translate, translation);
		}
	}
}

void Player::UpdateModel(bool allowInput, bool advanceState) {
	auto input = Input::GetInstance();
	bool modeChanged = false;
	// 編集停止中も、移動・戦闘処理を進めずに形態だけは確認できるようにする。
	if (allowInput) {
		const PlayerMode previousMode = currentMode_;
		if (input->TriggerAction(PlayerAction::TransformFighter)) ChangeMode(PlayerMode::Fighter);
		if (input->TriggerAction(PlayerAction::TransformGerwalk) ||
			input->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_LEFT)) ChangeMode(PlayerMode::Gerwalk);
		if (input->TriggerAction(PlayerAction::TransformBattroid)) ChangeMode(PlayerMode::Battroid);
		if (input->TriggerControllerButton(XINPUT_GAMEPAD_Y)) {
			PlayerMode nextMode = static_cast<PlayerMode>((static_cast<int>(currentMode_) + 1) % 3);
			ChangeMode(nextMode);
		}
		modeChanged = previousMode != currentMode_;
		if (modeChanged && !advanceState) {
			// 停止中は補間を進めないため、選んだ形態のポーズへ即座にスナップする。
			if (currentMode_ == PlayerMode::Fighter) {
				animationTime_ = 0.0f;
			} else if (currentMode_ == PlayerMode::Gerwalk) {
				animationTime_ = 1.71f;
			} else {
				animationTime_ = kBattroidIdleAnimationTime;
			}
			currentDrawScale_ = targetDrawScale_;
			presentationController_.SnapModeTransition();
		}
	}

	// Bキーでのガード状態に、左腕のポーズ用ブレンド値を同期する。
	// この時点で更新しておくことで、シールド・無敵判定・モデルの見た目が同じフレームで揃う。
	// ガードはガウォーク形態だけで可能とする。
	if (advanceState) {
		const bool canGuardInCurrentMode = GetModeParams(currentMode_).canGuard;
		isGuarding_ = allowInput && canGuardInCurrentMode && input->PushAction(PlayerAction::Guard) && !isSpecialAttackActive_;
		const float guardPoseTarget = isGuarding_ ? 1.0f : 0.0f;
		guardPoseWeight_ += (guardPoseTarget - guardPoseWeight_) * 0.22f;
	}

	// アニメーションの更新と適用
	if (animationData_.duration > 0.0f && !skeleton_.joints.empty()) {
		if (advanceState && !isAnimDebugActive_) {
			if (isPlayingAction_ && currentActionAnim_) {
				actionAnimTime_ += 1.0f / 60.0f;
				if (actionAnimTime_ > currentActionAnim_->duration) {
					isPlayingAction_ = false; // 再生完了
					currentActionAnim_ = nullptr;
				}
			} else {
				float modeTargetTime = 0.0f;
				if (currentMode_ == PlayerMode::Fighter) {
					modeTargetTime = 0.0f;
				} else if (currentMode_ == PlayerMode::Gerwalk) {
					modeTargetTime = 1.71f;  // Frame 41
				} else {
					modeTargetTime = 5.0f;   // kBattroidIdleAnimationTime
				}
				float diff = modeTargetTime - animationTime_;
				animationTime_ += diff * 0.1f;
			}
		} else if (advanceState) {
			if (overrideAnimation_ != nullptr) {
				// アニメーション編集ツール(カテゴリー5)等でカスタムアクションをデバッグ中の場合、
				// ベースアニメーション(変形)は現在のモード状態を維持する
				float modeTargetTime = 0.0f;
				if (currentMode_ == PlayerMode::Fighter) {
					modeTargetTime = 0.0f;
				} else if (currentMode_ == PlayerMode::Gerwalk) {
					modeTargetTime = 1.71f;
				} else {
					modeTargetTime = 5.0f;
				}
				float diff = modeTargetTime - animationTime_;
				animationTime_ += diff * 0.1f;
			} else {
				// プレイヤー設定(カテゴリー0)等でベースアニメーション自体をデバッグ中の場合
				animationTime_ = targetAnimationTime_;
			}
		}

		// 1. まずベースのアニメーション（変形アニメーション等）を全体に適用し、リセットする
		ApplyAnimation(skeleton_, animationData_, animationTime_);

		// 2. その上から、アクションやカスタムアニメーション（一部のボーンのみ等）を上書きで適用する
		const Animation* overrideAnimToApply = nullptr;
		float timeToApply = animationTime_;

		if (overrideAnimation_) {
			overrideAnimToApply = overrideAnimation_;
			if (isAnimDebugActive_) {
				timeToApply = targetAnimationTime_; // カスタムアニメの時間はシミュレーター指定時間
			}
		} else if (isPlayingAction_ && currentActionAnim_ && !isAnimDebugActive_) {
			overrideAnimToApply = currentActionAnim_;
			timeToApply = actionAnimTime_;
		}

		if (overrideAnimToApply && overrideAnimToApply->duration > 0.0f) {
			ApplyAnimation(skeleton_, *overrideAnimToApply, timeToApply);
		}

		if (!isAnimDebugActive_ && currentMode_ == PlayerMode::Battroid && isBattroidWalking_) {
			ApplyBattroidProceduralWalk();
		}
		if (!isAnimDebugActive_ && guardPoseWeight_ > 0.001f) {
			ApplyGuardPose(guardPoseWeight_);
		}
		::Update(skeleton_);

		if (enableSkinning_ && object_ && object_->GetModel()) {
			object_->UpdateSkinCluster(skeleton_);
		}
	}

	// スケールの滑らかな補間（アニメーション変形のように見せる）
	if (advanceState) {
		currentDrawScale_.x += (targetDrawScale_.x - currentDrawScale_.x) * 0.2f;
		currentDrawScale_.y += (targetDrawScale_.y - currentDrawScale_.y) * 0.2f;
		currentDrawScale_.z += (targetDrawScale_.z - currentDrawScale_.z) * 0.2f;
	}

	Quaternion visualQuaternion = quaternion_;
	if (dodgeTimer_ > 0) {
		const float progress = 1.0f - static_cast<float>(dodgeTimer_) / static_cast<float>(Player::kDodgeDurationFrames);
		const float rollAngle = progress * 6.2831853f * dodgeDirection_;
		visualQuaternion = MyMath::Normalize(MyMath::Multiply(
			quaternion_, MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, rollAngle)));
	}
	if (IsTransformPlayerModel() && (advanceState || modeChanged)) {
		presentationController_.UpdateTransformMech(
			object_.get(), static_cast<int>(currentMode_), position_, quaternion_, modelScale_,
			dodgeTimer_, Player::kDodgeDurationFrames, dodgeDirection_, guardPoseWeight_);
	} else if (object_) {
		object_->SetScale(currentDrawScale_);
		object_->SetTranslate(position_);
		object_->SetQuaternionRotate(visualQuaternion);
		object_->Update();
	}

	// Bキーを押している間だけガードを展開する。
	if (advanceState) {
		const float targetGuardScale = isGuarding_ ? 1.0f : 0.0f;
		guardScale_ += (targetGuardScale - guardScale_) * 0.25f;
	}

	// シールドが背景に溶けても状態を認識できるよう、ガード中は機体を青く発光させる。
	if (object_ && object_->GetModel() && !IsTransformPlayerModel()) {
		const float presentationAlpha = presentationController_.GetPresentationAlpha();
		Vector4 normalColor = UsesNaturalPlayerModelScale(modelName_)
			? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }
			: Vector4{ 0.2f, 0.5f, 1.0f, 1.0f };
		normalColor.w = presentationAlpha;
		object_->GetModel()->SetEnableLighting(!isGuarding_);
		object_->GetModel()->SetColor(isGuarding_
			? Vector4{ 0.15f, 0.75f, 1.0f, presentationAlpha }
			: normalColor);
	}

	presentationController_.UpdateGuardBarrier(
		advanceState, guardScale_, static_cast<int>(currentMode_), position_, quaternion_, GetForwardVector());
}

void Player::ApplyBattroidProceduralWalk() {
	const PlayerModeParams& p = modeParams_[static_cast<int>(PlayerMode::Battroid)];
	const float horizontalSpeed = Length({ velocity_.x, 0.0f, velocity_.z });
	const float speedRatio = std::clamp(horizontalSpeed / (std::max)(0.0001f, p.maxMoveSpeed), 0.35f, 1.0f);

	battroidWalkTime_ += (1.0f / 60.0f) * (0.8f + speedRatio * 1.2f);

	const float phase = battroidWalkTime_ * kBattroidWalkFrequency;
	const float step = std::sin(phase) * kBattroidFootStepDistance * speedRatio;
	const float footSwing = std::cos(phase) * kBattroidFootSwingAngle * speedRatio;
	const float leftLift = (std::max)(0.0f, std::sin(phase)) * kBattroidFootLiftDistance * speedRatio;
	const float rightLift = (std::max)(0.0f, -std::sin(phase)) * kBattroidFootLiftDistance * speedRatio;

	AddJointLocalTranslation(skeleton_, "FootFront", { 0.0f, rightLift, step });
	AddJointLocalTranslation(skeleton_, "FootRear", { 0.0f, rightLift, step });
	AddJointLocalTranslation(skeleton_, "FootGuardFront", { 0.0f, rightLift, step });
	AddJointLocalTranslation(skeleton_, "FootGuardRear", { 0.0f, rightLift, step });
	AddJointLocalRotation(skeleton_, "FootFront", { 1.0f, 0.0f, 0.0f }, -footSwing);
	AddJointLocalRotation(skeleton_, "FootRear", { 1.0f, 0.0f, 0.0f }, -footSwing * 0.7f);

	AddJointLocalTranslation(skeleton_, "FootFront.001", { 0.0f, leftLift, -step });
	AddJointLocalTranslation(skeleton_, "FootRear.001", { 0.0f, leftLift, -step });
	AddJointLocalTranslation(skeleton_, "FootGuardFront.001", { 0.0f, leftLift, -step });
	AddJointLocalTranslation(skeleton_, "FootGuardRear.001", { 0.0f, leftLift, -step });
	AddJointLocalRotation(skeleton_, "FootFront.001", { 1.0f, 0.0f, 0.0f }, footSwing);
	AddJointLocalRotation(skeleton_, "FootRear.001", { 1.0f, 0.0f, 0.0f }, footSwing * 0.7f);
}

void Player::ApplyGuardPose(float blendWeight) {
	// このモデルでは .001 側がゲーム画面の右腕だったため、左腕の実体である
	// 非 .001 側を操作する。各部は独立した剛体メッシュなので、手首だけを
	// 動かすのではなく、肩→肘→手首の順に支点回転させる。
	const float weight = std::clamp(blendWeight, 0.0f, 1.0f);
	const std::initializer_list<const char*> kLeftArm = {
		"Shoulder_29", "ShoulderJoint1_28", "UpperArm_30", "Elbow_21", "Forearm_22", "Hand_23",
		"Cube.040_61", "Cube.021_75", "Cube.017_74", "Cube.039_78", "Cube.013_51",
		"Cube.018_56", "Cube.015_55", "Cube.022_57", "Cube.001_52", "Cube.042_79",
		"Cube.020_66", "Cube.041_70", "Cube.006_3", "Cube.016_65", "Cube.038_69",
		"Cube.012_50", "Cube.019_95",
	};
	const std::initializer_list<const char*> kLeftElbowToHand = {
		"Elbow_21", "Forearm_22", "Hand_23", "Cube.040_61", "Cube.021_75", "Cube.017_74",
		"Cube.039_78", "Cube.013_51", "Cube.018_56", "Cube.015_55", "Cube.022_57",
		"Cube.001_52", "Cube.042_79", "Cube.020_66", "Cube.041_70", "Cube.006_3",
		"Cube.016_65", "Cube.038_69", "Cube.012_50", "Cube.019_95",
	};
	const std::initializer_list<const char*> kLeftHand = {
		"Hand_23", "Cube.040_61", "Cube.021_75", "Cube.017_74", "Cube.039_78", "Cube.013_51",
		"Cube.018_56", "Cube.015_55", "Cube.022_57", "Cube.001_52", "Cube.042_79",
		"Cube.020_66", "Cube.041_70", "Cube.006_3", "Cube.016_65", "Cube.038_69",
		"Cube.012_50", "Cube.019_95",
	};
	const std::initializer_list<const char*> kRightHandWeapon = {
		"GunGrip_100", "BarrelBack_98", "barrelFront_99",
	};

	// マクロスΔスクランブル風のバトロイドガード。モデルの前方は -Z。
	// 左上腕を前方かつ内側へ入れ、肘を大きく畳んで前腕をコクピット前に
	// 立てる。銃身はソース内で独立ノードになっているので、右手へ固定する。
	// 右手の装備は左腕ガードの回転対象に入れない。
	const Vector3 rightHandRest = GetJointTranslation(skeleton_, "Hand.001_46");
	const Vector3 gunGripRest = GetJointTranslation(skeleton_, "GunGrip_100");
	const Vector3 weaponAttachOffset = {
		(rightHandRest.x - gunGripRest.x) * weight,
		(rightHandRest.y - gunGripRest.y) * weight,
		(rightHandRest.z - gunGripRest.z) * weight,
	};
	TranslateJointGroup(skeleton_, kRightHandWeapon, weaponAttachOffset);

	const Vector3 shoulderPivot = GetJointTranslation(skeleton_, "Shoulder_29");
	RotateJointGroupAroundPivot(skeleton_, kLeftArm, shoulderPivot, { 1.0f, 0.0f, 0.0f }, 0.42f * weight);
	RotateJointGroupAroundPivot(skeleton_, kLeftArm, shoulderPivot, { 0.0f, 1.0f, 0.0f }, 0.32f * weight);
	RotateJointGroupAroundPivot(skeleton_, kLeftArm, shoulderPivot, { 0.0f, 0.0f, 1.0f }, 0.65f * weight);

	const Vector3 elbowPivot = GetJointTranslation(skeleton_, "Elbow_21");
	RotateJointGroupAroundPivot(skeleton_, kLeftElbowToHand, elbowPivot, { 1.0f, 0.0f, 0.0f }, 1.34f * weight);

	const Vector3 wristPivot = GetJointTranslation(skeleton_, "Hand_23");
	RotateJointGroupAroundPivot(skeleton_, kLeftHand, wristPivot, { 1.0f, 0.0f, 0.0f }, -0.20f * weight);
	TranslateJointGroup(skeleton_, kLeftHand, { 0.0f, -0.45f * weight, -0.25f * weight });
}
