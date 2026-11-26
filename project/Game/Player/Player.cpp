#include "Player.h"
#include "Game/base/GameSettings.h"
#include "3D/Object3dCommon.h"
#include "3D/ModelManager.h"
#include "Game/obstacle/Obstacle.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <initializer_list>

namespace {
	constexpr const char *kDefaultPlayerBoxModelName = "PlayerBox";
	constexpr const char *kTransformPlayerModelName = "TransformFighter";
	constexpr float kBattroidGravity = 0.006f;
	constexpr float kBattroidMaxFallSpeed = 0.16f;
	constexpr float kBattroidIdleAnimationTime = 5.0f;
	constexpr float kBattroidWalkFrequency = 4.5f;
	constexpr float kBattroidFootSwingAngle = 0.14f;
	constexpr float kBattroidFootStepDistance = 0.18f;
	constexpr float kBattroidFootLiftDistance = 0.08f;
	// 必殺技中にロック対象が変わった瞬間の視線・機体姿勢の飛びを抑える。
	// 1フレームあたりの追従率なので、約0.3秒でほぼ新しい対象へ向く。
	constexpr float kSpecialAttackTargetTurnBlend = 0.16f;

	bool UsesNaturalPlayerModelScale(const std::string &modelName) {
		return modelName != kDefaultPlayerBoxModelName;
	}

	bool IsTransformPlayerModelName(const std::string& modelName) {
		return modelName == kTransformPlayerModelName;
	}

	float LengthSq(const Vector3 &value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float Length(const Vector3 &value) {
		return std::sqrt(LengthSq(value));
	}

	Vector3 Add(const Vector3 &a, const Vector3 &b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vector3 Scale(const Vector3 &value, float scalar) {
		return { value.x * scalar, value.y * scalar, value.z * scalar };
	}

	Quaternion MakeLocalRotation(float x, float y, float z) {
		const Quaternion qx = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, x);
		const Quaternion qy = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, y);
		const Quaternion qz = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, z);
		return MyMath::Normalize(MyMath::Multiply(MyMath::Multiply(qx, qy), qz));
	}

	float Dot(const Vector3 &a, const Vector3 &b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	Vector3 NormalizeOrZero(const Vector3 &value) {
		const float length = Length(value);
		if (length <= 0.0001f) {
			return { 0.0f, 0.0f, 0.0f };
		}
		return Scale(value, 1.0f / length);
	}

	Vector3 ClampLength(const Vector3 &value, float maxLength) {
		const float length = Length(value);
		if (length <= maxLength || length <= 0.0001f) {
			return value;
		}
		return Scale(value, maxLength / length);
	}

	Quaternion MakeNoRollLookQuaternion(const Vector3 &forward) {
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

	Quaternion MakeYawQuaternionFromForward(const Vector3 &forward) {
		Vector3 flatForward = NormalizeOrZero({ forward.x, 0.0f, forward.z });
		if (LengthSq(flatForward) <= 0.0001f) {
			flatForward = { 0.0f, 0.0f, 1.0f };
		}

		const float yaw = std::atan2(flatForward.x, flatForward.z);
		return MyMath::Normalize(MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw));
	}

	float CameraPitchFromForward(const Vector3 &forward) {
		Vector3 normalizedForward = NormalizeOrZero(forward);
		if (LengthSq(normalizedForward) <= 0.0001f) {
			return 0.0f;
		}
		return std::clamp(-std::asin(std::clamp(normalizedForward.y, -1.0f, 1.0f)), -1.2f, 1.2f);
	}

	float BlendAngleShortestPath(float current, float target, float blend) {
		constexpr float kTwoPi = 6.28318530717958647692f;
		const float shortestDifference = std::remainder(target - current, kTwoPi);
		return current + shortestDifference * std::clamp(blend, 0.0f, 1.0f);
	}
}

void Player::Initialize(const std::string &modelName) {
	modelName_ = modelName;
	const bool isTransformPlayer = IsTransformPlayerModel();

	if (modelName.find("vf-15c") != std::string::npos || isTransformPlayer) {
		modelScale_ = { 0.08f, 0.08f, 0.08f }; // スケールを0.08に調整
	} else {
		modelScale_ = { 1.0f, 1.0f, 1.0f };
	}

	if (isTransformPlayer) {
		object_ = presentationController_.InitializeTransformMechModel();
	} else {
		// モデルマネージャに読み込みを指示
		ModelManager::GetInstance()->LoadModel(modelName);
		const bool usesNaturalScale = UsesNaturalPlayerModelScale(modelName_);

		object_ = std::make_unique<Object3d>();
		object_->Initialize(Object3dCommon::GetInstance());
		object_->SetModel(modelName);

		if (object_->GetModel()) {
			object_->GetModel()->SetColor(usesNaturalScale ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : Vector4{ 0.2f, 0.5f, 1.0f, 1.0f });
		}
	}
	currentDrawScale_ = modelScale_;
	targetDrawScale_ = modelScale_;
	object_->SetScale(currentDrawScale_);

	presentationController_.InitializeGuardBarrier();

	position_ = { 0.0f, 0.0f, 0.0f };
	velocity_ = { 0.0f, 0.0f, 0.0f };
	quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	cameraPitch_ = 0.0f;
	lastCameraDirection_ = { 0.0f, 0.0f, 1.0f };
	cameraFollowPosition_ = { 0.0f, 0.0f, 0.0f };
	isCameraFollowInitialized_ = false;
	wasCameraLockOnActive_ = false;
	cameraLockReleaseProgress_ = 1.0f;
	cameraLockReleaseDirection_ = { 0.0f, 0.0f, 1.0f };
	cameraLockReleaseFocusOffset_ = { 0.0f, 0.0f, 0.0f };
	hp_ = kMaxHP;
	isDead_ = false;
	isSpecialAttackActive_ = false;
	dodgeTimer_ = 0;
	dodgeCooldownTimer_ = 0;
	dodgeDirection_ = 0.0f;
	isGuarding_ = false;
	guardScale_ = 0.0f;
	guardPoseWeight_ = 0.0f;
	animationTime_ = 0.0f;
	targetAnimationTime_ = 0.0f;
	battroidWalkTime_ = 0.0f;
	currentActionAnim_ = nullptr;
	actionAnimTime_ = 0.0f;
	isPlayingAction_ = false;
	overrideAnimation_ = nullptr;
	presentationController_.SetPresentationAlpha(1.0f);
	presentationController_.SetPresentationThrustersVisible(true);

	// アニメーションデータのロード
	if (modelName.find("vf-15c") != std::string::npos) {
		std::string dir = "resources/vf-15c";
		std::string file = "scene.gltf";
		animationData_ = LoadAnimationFile(dir, file);
		
		Node rootNode;
		if (object_ && object_->GetModel()) {
			rootNode = object_->GetModel()->GetModelData().rootNode;
		} else {
			rootNode = Model::LoadNodeHierarchy(dir, file);
		}
		skeleton_ = CreateSkeleton(rootNode);
		
		if (!skeleton_.joints.empty()) {
			enableSkinning_ = true;
			if (object_ && object_->GetModel()) {
				object_->CreateSkinCluster(skeleton_);
			}
		}
	}

	// アクションアニメーションの自動ロード
	actionAnimations_.clear();
    // モードパラメータの初期化
    // ファイター（ミサイル乱射特化）
	// 高速・慣性重視。Wでの加速と回避の伸びを明確にする。
	modeParams_[0].maxMoveSpeed = 0.24f;
	modeParams_[0].moveAcceleration = 0.020f;
	modeParams_[0].moveDamping = 0.96f;
	modeParams_[0].pitchSpeed = 0.018f;
	modeParams_[0].yawSpeed = 0.017f;
	modeParams_[0].rollSpeed = 0.030f;
    modeParams_[0].canGuard = false;
    modeParams_[0].canMelee = false;
    modeParams_[0].maxMultiLock = 8;
    modeParams_[0].lockOnAngleDot = 0.5f;
    modeParams_[0].maxLockOnDistance = 200.0f;

    // ガウォーク（広角ロック標準形態）
	// 低速・即停止重視。横移動と上下移動で照準を維持する役割を明確にする。
	modeParams_[1].maxMoveSpeed = 0.14f;
	modeParams_[1].moveAcceleration = 0.030f;
	modeParams_[1].moveDamping = 0.76f;
	modeParams_[1].pitchSpeed = 0.020f;
	modeParams_[1].yawSpeed = 0.024f;
	modeParams_[1].rollSpeed = 0.010f;
    modeParams_[1].canGuard = true;
    modeParams_[1].canMelee = false;
    modeParams_[1].maxMultiLock = 4;
    modeParams_[1].lockOnAngleDot = -0.2f;
    modeParams_[1].maxLockOnDistance = 100.0f;

    // バトロイド（防御＆格闘特化）
    modeParams_[2].moveDamping = 0.80f;
    modeParams_[2].pitchSpeed = 0.02f;
    modeParams_[2].yawSpeed = 0.02f;
    modeParams_[2].rollSpeed = 0.02f;
    modeParams_[2].canGuard = true;
    modeParams_[2].canMelee = true;
    modeParams_[2].maxMultiLock = 2;
    modeParams_[2].lockOnAngleDot = -1.0f;
    modeParams_[2].maxLockOnDistance = 60.0f;

    // 初期形態
    ChangeMode(PlayerMode::Fighter);

    boosterEffect_ = std::make_unique<BoosterEffect>();
    boosterEffect_->Initialize();
    boosterEffect_->SetPlayerModelScale(modelScale_.x);
	// 初回 Draw の前に、初期姿勢・スキニング行列・Object3d 行列を同じ状態に揃える。
	// これを行わないと、シーン切替直後の 1 フレームだけ初期ポーズが表示される。
	UpdateModel(false, false);
}

void Player::ChangeMode(PlayerMode newMode) {
	// バトロイド形態は調整中のため、外部からの切替も含めて一時的に無効化する。
	if (newMode == PlayerMode::Battroid) {
		return;
	}

	const PlayerMode previousMode = currentMode_;
	if (newMode != PlayerMode::Fighter) {
		// ガウォーク／バトロイドへ変形した時点でファイター専用回避を終了する。
		dodgeTimer_ = 0;
		dodgeDirection_ = 0.0f;
	} else {
		isGuarding_ = false;
	}

	if (newMode != PlayerMode::Fighter) {
		const Vector3 currentForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
		cameraPitch_ = CameraPitchFromForward(currentForward);
		quaternion_ = MakeYawQuaternionFromForward(currentForward);
	}

    currentMode_ = newMode;
    if (IsTransformPlayerModel() && previousMode != newMode) {
        presentationController_.BeginModeTransition(static_cast<int>(previousMode));
    }
    const bool usesNaturalScale = UsesNaturalPlayerModelScale(modelName_);
    
    // 形態ごとの見た目の変更（スケールを変えて滑らかに変形させる）
    if (usesNaturalScale) {
        // vf-15cのようなちゃんとしたモデルの場合は、勝手にスケールを歪ませて形を崩さない
        targetDrawScale_ = modelScale_;
    } else {
        // 仮モデルの場合はスケールで形態を表現
	    if (currentMode_ == PlayerMode::Fighter) {
	        targetDrawScale_ = { modelScale_.x * 1.0f, modelScale_.y * 0.5f, modelScale_.z * 1.5f };
	    } else if (currentMode_ == PlayerMode::Gerwalk) {
	        targetDrawScale_ = { modelScale_.x * 1.0f, modelScale_.y * 1.0f, modelScale_.z * 1.0f };
	    } else if (currentMode_ == PlayerMode::Battroid) {
	        targetDrawScale_ = { modelScale_.x * 0.8f, modelScale_.y * 1.5f, modelScale_.z * 0.8f };
	    }
    }
}

void Player::SetModeParams(PlayerMode mode, const PlayerModeParams& params) {
	PlayerModeParams validated = params;
	validated.maxMoveSpeed = std::clamp(validated.maxMoveSpeed, 0.01f, 1.0f);
	validated.moveAcceleration = std::clamp(validated.moveAcceleration, 0.001f, 0.1f);
	validated.moveDamping = std::clamp(validated.moveDamping, 0.1f, 1.0f);
	validated.pitchSpeed = std::clamp(validated.pitchSpeed, 0.001f, 0.1f);
	validated.yawSpeed = std::clamp(validated.yawSpeed, 0.001f, 0.1f);
	validated.rollSpeed = std::clamp(validated.rollSpeed, 0.001f, 0.1f);
	validated.maxMultiLock = std::clamp(validated.maxMultiLock, 1, 12);
	validated.lockOnAngleDot = std::clamp(validated.lockOnAngleDot, -1.0f, 1.0f);
	validated.maxLockOnDistance = std::clamp(validated.maxLockOnDistance, 1.0f, 1000.0f);
	modeParams_[static_cast<int>(mode)] = validated;
}

bool Player::IsTransformPlayerModel() const {
	return IsTransformPlayerModelName(modelName_);
}

void Player::Update(const std::list<std::unique_ptr<Obstacle>> &obstacles, const Vector3 *lockOnTarget) {
	if (isDead_) return;

	isNearBoundary_ = false;
	boundaryWarningIntensity_ = 0.0f;
	isNearWallBoundary_ = false;
	wallBoundaryWarningIntensity_ = 0.0f;
	isNearCeilingBoundary_ = false;
	ceilingBoundaryWarningIntensity_ = 0.0f;

	if (isSpecialAttackActive_) {
		isGuarding_ = false;
		// 必殺技中はその場に固定し、慣性による移動も止める。
		velocity_ = { 0.0f, 0.0f, 0.0f };

		if (lockOnTarget) {
			// 必殺技中はロックオンしている敵へ追従する。対象を切り替えた際も
			// 最短方向に補間し、照準・カメラ・機体の向きが一度に飛ばないようにする。
			Vector3 toTarget = { lockOnTarget->x - position_.x, lockOnTarget->y - position_.y, lockOnTarget->z - position_.z };
			if (LengthSq(toTarget) > 0.0001f) {
				Vector3 dir = NormalizeOrZero(toTarget);
				const float targetYaw = std::atan2(dir.x, dir.z);
				const float targetPitch = CameraPitchFromForward(dir);
				specialAttackCameraYaw_ = BlendAngleShortestPath(
					specialAttackCameraYaw_, targetYaw, kSpecialAttackTargetTurnBlend);
				specialAttackCameraPitch_ +=
					(targetPitch - specialAttackCameraPitch_) * kSpecialAttackTargetTurnBlend;
				quaternion_ = MakeNoRollLookQuaternion(GetAttackDirection());
			}
		} else {
			// 機体は固定したまま、ロックオン対象が無い場合のみ手動調整
			Input *input = Input::GetInstance();
			constexpr float kSpecialCameraKeySpeed = 0.025f;
			const float specialCameraMouseSensitivity = GameSettings::GetInstance().GetMouseSensitivity();
			if (input->PushAction(PlayerAction::TurnLeft)) specialAttackCameraYaw_ -= kSpecialCameraKeySpeed;
			if (input->PushAction(PlayerAction::TurnRight)) specialAttackCameraYaw_ += kSpecialCameraKeySpeed;
			if (input->PushAction(PlayerAction::PitchUp)) specialAttackCameraPitch_ -= kSpecialCameraKeySpeed;
			if (input->PushAction(PlayerAction::PitchDown)) specialAttackCameraPitch_ += kSpecialCameraKeySpeed;
			specialAttackCameraYaw_ += static_cast<float>(input->GetMouseDeltaX()) * specialCameraMouseSensitivity;
			specialAttackCameraPitch_ += static_cast<float>(input->GetMouseDeltaY()) * specialCameraMouseSensitivity;
			specialAttackCameraPitch_ = std::clamp(specialAttackCameraPitch_, -1.2f, 1.2f);

			Vector3 aimDir = GetAttackDirection();
			quaternion_ = MakeNoRollLookQuaternion(aimDir);
		}
	} else {
		if (lockOnTarget) {
			UpdateLockOnRotation(*lockOnTarget);
		}
		Move(lockOnTarget != nullptr);
		CheckCollision(obstacles);
	}

	// 変形・回避ロールを含む描画姿勢を先に確定する。
	UpdateModel();
	if (boosterEffect_ && presentationController_.ArePresentationThrustersVisible()) {
		auto input = Input::GetInstance();
		PlayerModeParams& p = modeParams_[static_cast<int>(currentMode_)];
		float speedVal = Length(velocity_);
		float speedRatio = speedVal / (std::max)(0.0001f, p.maxMoveSpeed);
		bool isAccelerating = input->PushAction(PlayerAction::MoveForward);
		Vector3 thrusterMovementIntent = { 0.0f, 0.0f, 0.0f };
		if (currentMode_ == PlayerMode::Gerwalk) {
			if (input->PushAction(PlayerAction::MoveForward)) thrusterMovementIntent.z += 1.0f;
			if (input->PushAction(PlayerAction::MoveBackward)) thrusterMovementIntent.z -= 1.0f;
			if (input->PushAction(PlayerAction::MoveUp)) thrusterMovementIntent.y += 1.0f;
		}
		boosterEffect_->Update(position_, object_ ? object_->GetQuaternionRotate() : quaternion_,
			static_cast<int>(currentMode_), speedRatio, isAccelerating, thrusterMovementIntent);
	}
}

void Player::UpdatePresentation(const Vector3& position, const Vector3& eulerRotation,
	float speed, bool isBoosting) {
	position_ = position;
	SetRotation(eulerRotation);
	// タイトルなどの自動飛行も、機体の最終姿勢と噴射姿勢を揃える。
	UpdateModel(false);

	if (boosterEffect_ && presentationController_.ArePresentationThrustersVisible()) {
		const PlayerModeParams& params = modeParams_[static_cast<int>(currentMode_)];
		const float speedRatio = speed / (std::max)(0.0001f, params.maxMoveSpeed);
		boosterEffect_->Update(position_, object_ ? object_->GetQuaternionRotate() : quaternion_,
			static_cast<int>(currentMode_), speedRatio, isBoosting);
	}
}

void Player::SetPresentationClipPlane(const Vector3& normal, float distance) {
	presentationController_.SetClipPlane(object_.get(), normal, distance);
}

void Player::ClearPresentationClipPlane() {
	presentationController_.ClearClipPlane(object_.get());
}

void Player::Draw(Camera* camera, bool drawEffects) {
	const bool isFadingIn = presentationController_.GetPresentationAlpha() < 0.999f;
	if (isFadingIn) {
		Object3dCommon::GetInstance()->SetAlphaBlendDrawSettings();
	}
	if (object_) {
		// Scene のカメラ更新後の行列で、機体と炎を同じフレームに投影する。
		object_->Update(camera);
		object_->Draw();
	}
	presentationController_.DrawTransformParts(camera);
	if (isFadingIn) {
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
	}
	if (boosterEffect_ && presentationController_.ArePresentationThrustersVisible()) {
		boosterEffect_->RefreshPlacement(object_ ? object_->GetTranslate() : position_,
			object_ ? object_->GetQuaternionRotate() : quaternion_, static_cast<int>(currentMode_));
		boosterEffect_->DrawNozzles(camera);
	}
	if (drawEffects) {
		DrawEffects(camera);
	}
}

void Player::DrawEffects(Camera* camera) {
	presentationController_.DrawGuardBarrier(camera, guardScale_);
	if (boosterEffect_ && presentationController_.ArePresentationThrustersVisible()) {
		// 不透明パスで実際に描いた機体の位置・姿勢を、そのまま炎の親変換に使う。
		boosterEffect_->RefreshPlacement(object_ ? object_->GetTranslate() : position_,
			object_ ? object_->GetQuaternionRotate() : quaternion_, static_cast<int>(currentMode_));
		// スラスターだけは加算合成で描く。機体本体の通常描画状態を
		// 引き継がないことで、半透明の炎が暗く埋もれないようにする。
		Object3dCommon::GetInstance()->SetEffectDrawSettings();
		boosterEffect_->Draw(camera);
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
	}
}

void Player::SetRotation(const Vector3 &eulerRotation) {
	Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, eulerRotation.x);
	Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, eulerRotation.y);
	Quaternion qRoll = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, eulerRotation.z);

	quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	quaternion_ = MyMath::Multiply(quaternion_, qPitch);
	quaternion_ = MyMath::Multiply(quaternion_, qYaw);
	quaternion_ = MyMath::Multiply(quaternion_, qRoll);
	quaternion_ = MyMath::Normalize(quaternion_);
	if (currentMode_ != PlayerMode::Fighter) {
		const Vector3 currentForward = MyMath::RotateVector({ 0.0f, 0.0f, 1.0f }, quaternion_);
		cameraPitch_ = CameraPitchFromForward(currentForward);
		quaternion_ = MakeYawQuaternionFromForward(currentForward);
	}

	if (object_) {
		object_->SetQuaternionRotate(quaternion_);
		object_->Update();
	}
}

void Player::OnCollision() {
	if (isSpecialAttackActive_ || IsDodging() || IsGuarding()) return;
	Destroy();
}

void Player::PlayActionAnimation(const std::string& actionName) {
    if (actionAnimations_.count(actionName)) {
        currentActionAnim_ = &actionAnimations_[actionName];
        actionAnimTime_ = 0.0f;
        isPlayingAction_ = true;
    }
}

void Player::StopActionAnimation() {
    isPlayingAction_ = false;
    currentActionAnim_ = nullptr;
}

OBB Player::GetMeleeHitbox() const {
	OBB obb = GetOBB();
	// 前方に判定を出す
	Vector3 forward = GetForwardVector();
	// 攻撃範囲のサイズ設定
	obb.size.x += 4.0f; // 横に広く
	obb.size.y += 4.0f; // 縦に広く
	obb.size.z += 8.0f; // 前方に長く

	// 中心位置を前方にずらす
	obb.center.x += forward.x * 6.0f;
	obb.center.y += forward.y * 6.0f;
	obb.center.z += forward.z * 6.0f;
	return obb;
}