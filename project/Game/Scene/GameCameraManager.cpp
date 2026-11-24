#include "GameCameraManager.h"
#include "Game/Player/Player.h"
#include "engine/Input/Input.h"
#include "engine/Debug/ImGuiManager.h"
#include <externals/imgui/imgui.h>
#include <algorithm>
#include <cmath>

namespace {
	constexpr float kNormalCameraFovY = 0.45f;
	constexpr float kNormalCameraFarClip = 3000.0f;
	constexpr float kCinematicCameraFovY = 0.70f;
	constexpr float kCinematicCameraFarClip = 3000.0f;
	constexpr float kCinematicCameraFocusBlend = 0.18f;
	constexpr float kCinematicCameraDirectionBlend = 0.10f;
	constexpr float kCinematicCameraPositionBlend = 0.10f;
	constexpr float kCinematicCameraRotationBlend = 0.12f;

	Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
		return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
	}

	Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
		return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
	}

	Vector3 ScaleVector3(const Vector3& value, float scale) {
		return { value.x * scale, value.y * scale, value.z * scale };
	}

	Vector3 FlattenYVector3(const Vector3& value) {
		return { value.x, 0.0f, value.z };
	}

	float LengthSqVector3(const Vector3& value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float LengthVector3(const Vector3& value) {
		return std::sqrt(LengthSqVector3(value));
	}

	Vector3 NormalizeOrVector3(const Vector3& value, const Vector3& fallback) {
		const float length = LengthVector3(value);
		if (length <= 0.0001f) {
			return fallback;
		}
		return ScaleVector3(value, 1.0f / length);
	}

	Vector3 LerpVector3(const Vector3& from, const Vector3& to, float t) {
		return {
			from.x + (to.x - from.x) * t,
			from.y + (to.y - from.y) * t,
			from.z + (to.z - from.z) * t
		};
	}

	Quaternion MakeLookQuaternion(const Vector3& forward) {
		const Vector3 normalizedForward = NormalizeOrVector3(forward, { 0.0f, 0.0f, 1.0f });
		const float clampedY = std::clamp(normalizedForward.y, -1.0f, 1.0f);
		const float pitch = -std::asin(clampedY);
		const float yaw = std::atan2(normalizedForward.x, normalizedForward.z);

		Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
		return MyMath::Normalize(MyMath::Multiply(qYaw, qPitch));
	}
}

GameCameraManager::GameCameraManager() {
}

void GameCameraManager::Initialize() {
	// カメラの初期化
	camera_ = std::make_unique<Camera>();

	// デバッグ用カメラの初期化
	debugFlyCamera_ = std::make_unique<FlyCamera>();
}

Camera* GameCameraManager::GetActiveCamera() const {
	if (isDebugCameraActive_) {
		return static_cast<Camera*>(debugFlyCamera_.get());
	}
	return camera_.get();
}

void GameCameraManager::Update(Enemy* lockedEnemy, const Vector3& playerPosition) {
	if (isDebugCameraActive_) {
		// FlyCameraが自分で入力を消化して自分を更新する
		debugFlyCamera_->Update();
	}
	else {
		bool canUseCinematicCamera =
			isCinematicLockOnCameraEnabled_ &&
			lockedEnemy &&
			!lockedEnemy->IsDead();

		if (canUseCinematicCamera) {
			UpdateCinematicLockOnCamera(camera_.get(), lockedEnemy, playerPosition);
		}
		else {
			UpdateGameplayCamera(playerPosition);
		}
		camera_->Update();
	}
}

void GameCameraManager::UpdateGameplayCamera(const Vector3& playerPosition) {
	if (!camera_) {
		return;
	}

	isCinematicLockOnCameraInitialized_ = false;
	// camera_->SetFovY(kNormalCameraFovY); // Let Player handle it
	camera_->SetFarClip(kNormalCameraFarClip);

	// 注意：元々はPlayerクラス内でCameraとTargetを同期していたため、
	// 呼び出し元(PlayerのUpdate等)でCameraの位置を更新する必要があります。
}

void GameCameraManager::UpdateCinematicLockOnCamera(Camera* targetCamera, Enemy* lockedEnemy, const Vector3& playerPosition) {
	if (!targetCamera || !lockedEnemy) {
		return;
	}

	const Vector3 enemyPosition = lockedEnemy->GetPosition();
	const float enemyRadius = lockedEnemy->GetCollisionRadius();

	Vector3 playerFocus = playerPosition;
	playerFocus.y += 1.0f;

	Vector3 enemyFocus = enemyPosition;
	enemyFocus.y += enemyRadius * 0.35f;

	Vector3 rawFocus = ScaleVector3(AddVector3(playerFocus, enemyFocus), 0.5f);
	const Vector3 toEnemy = SubtractVector3(enemyFocus, playerFocus);
	const float separation = LengthVector3(toEnemy);
	
	// プレイヤーの向いている方向が必要なため近似（toEnemyをベースに）
	const Vector3 playerForward = NormalizeOrVector3(toEnemy, { 0.0f, 0.0f, 1.0f });
	const Vector3 enemyDirection = NormalizeOrVector3(toEnemy, playerForward);
	const Vector3 flatPlayerForward = NormalizeOrVector3(FlattenYVector3(playerForward), { 0.0f, 0.0f, 1.0f });
	const Vector3 flatEnemyDirection = NormalizeOrVector3(FlattenYVector3(enemyDirection), flatPlayerForward);

	Vector3 rawCameraBackDirection = NormalizeOrVector3(
		AddVector3(
			ScaleVector3(flatPlayerForward, 0.65f),
			ScaleVector3(flatEnemyDirection, 0.35f)),
		flatEnemyDirection);

	const Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
	Vector3 rawSideDirection = MyMath::Cross(worldUp, rawCameraBackDirection);
	rawSideDirection = NormalizeOrVector3(rawSideDirection, { 1.0f, 0.0f, 0.0f });

	if (!isCinematicLockOnCameraInitialized_) {
		cinematicLockOnCameraPosition_ = targetCamera->GetTranslate();
		cinematicLockOnCameraRotation_ = MakeLookQuaternion(playerForward);
		cinematicLockOnCameraFocus_ = rawFocus;
		cinematicLockOnCameraBackDirection_ = rawCameraBackDirection;
		cinematicLockOnCameraSeparation_ = separation;
		cinematicLockOnCameraSideSign_ =
			(MyMath::Dot(SubtractVector3(targetCamera->GetTranslate(), rawFocus), rawSideDirection) < 0.0f) ? -1.0f : 1.0f;
		isCinematicLockOnCameraInitialized_ = true;
	} else {
		cinematicLockOnCameraFocus_ = LerpVector3(cinematicLockOnCameraFocus_, rawFocus, kCinematicCameraFocusBlend);
		cinematicLockOnCameraBackDirection_ = NormalizeOrVector3(
			LerpVector3(cinematicLockOnCameraBackDirection_, rawCameraBackDirection, kCinematicCameraDirectionBlend),
			rawCameraBackDirection);
		cinematicLockOnCameraSeparation_ += (separation - cinematicLockOnCameraSeparation_) * kCinematicCameraFocusBlend;
	}

	const Vector3 focus = cinematicLockOnCameraFocus_;
	const Vector3 cameraBackDirection = cinematicLockOnCameraBackDirection_;
	const float cameraSeparation = cinematicLockOnCameraSeparation_;
	Vector3 sideDirection = MyMath::Cross(worldUp, cameraBackDirection);
	sideDirection = NormalizeOrVector3(sideDirection, rawSideDirection);
	sideDirection = ScaleVector3(sideDirection, cinematicLockOnCameraSideSign_);

	const int32_t clientWidth = WinApp::GetClientWidth();
	const int32_t clientHeight = WinApp::GetClientHeight();
	const float aspectRatio = (clientWidth > 0 && clientHeight > 0)
		? static_cast<float>(clientWidth) / static_cast<float>(clientHeight)
		: 16.0f / 9.0f;
	const float horizontalFov = 2.0f * std::atan(std::tan(kCinematicCameraFovY * 0.5f) * aspectRatio);
	const float fitDistance = (cameraSeparation * 0.55f + 4.0f) / std::tan(horizontalFov * 0.5f);
	const float distanceBase = 16.0f + cameraSeparation * 0.45f;
	const float fitBackDistance = fitDistance + 8.0f;
	const float desiredBackDistance = (distanceBase > fitBackDistance) ? distanceBase : fitBackDistance;
	const float backDistance = std::clamp(desiredBackDistance, 16.0f, 75.0f);
	const float sideOffset = std::clamp(cameraSeparation * 0.22f, 2.0f, 10.0f);
	const float heightOffset = std::clamp(4.0f + cameraSeparation * 0.18f, 5.0f, 16.0f);

	Vector3 desiredPosition = focus;
	desiredPosition = AddVector3(desiredPosition, ScaleVector3(cameraBackDirection, -backDistance));
	desiredPosition = AddVector3(desiredPosition, ScaleVector3(sideDirection, sideOffset));
	desiredPosition.y += heightOffset;

	Vector3 lookTarget = focus;
	lookTarget.y += std::clamp(cameraSeparation * 0.04f, 0.5f, 2.5f);
	const Vector3 lookForward = NormalizeOrVector3(SubtractVector3(lookTarget, desiredPosition), cameraBackDirection);
	const Quaternion desiredRotation = MakeLookQuaternion(lookForward);

	cinematicLockOnCameraPosition_ = LerpVector3(cinematicLockOnCameraPosition_, desiredPosition, kCinematicCameraPositionBlend);
	cinematicLockOnCameraRotation_ = MyMath::Slerp(cinematicLockOnCameraRotation_, desiredRotation, kCinematicCameraRotationBlend);

	targetCamera->SetFovY(kCinematicCameraFovY);
	targetCamera->SetFarClip(kCinematicCameraFarClip);
	targetCamera->SetTranslate(cinematicLockOnCameraPosition_);
	targetCamera->SetQuaternion(cinematicLockOnCameraRotation_);
}

