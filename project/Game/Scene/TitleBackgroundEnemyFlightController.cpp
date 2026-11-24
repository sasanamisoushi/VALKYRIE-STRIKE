#include "TitleBackgroundEnemyFlightController.h"

#include "TitleBackgroundEnemyState.h"
#include "engine/Camera/Camera.h"
#include "engine/base/WinApp.h"
#include "engine/math/MyMath.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h"

#include <algorithm>
#include <cmath>

namespace {
	Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
		return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
	}

	Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) {
		const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
		if (length <= 0.0001f) {
			return fallback;
		}
		return { value.x / length, value.y / length, value.z / length };
	}
}

void TitleBackgroundEnemyFlightController::Update(Player& player, Enemy& enemy, Camera& camera,
	const std::list<std::unique_ptr<Obstacle>>& obstacles, TitleBackgroundEnemyState& state,
	int titleBackgroundFrame) const {
	const float time = static_cast<float>(titleBackgroundFrame) * (1.0f / 60.0f);
	const Vector3 forward = NormalizeOr(player.GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	const Vector3 right = NormalizeOr(MyMath::Cross({ 0.0f, 1.0f, 0.0f }, forward), { 1.0f, 0.0f, 0.0f });
	const Vector3 up = NormalizeOr(MyMath::Cross(forward, right), { 0.0f, 1.0f, 0.0f });

	Vector3 targetPosition = player.GetPosition();
	const float forwardDistance = 25.0f + std::sin(time * 0.7f) * 4.0f;
	const float lateralDistance = std::sin(time * 1.15f) * 7.0f;
	targetPosition.x += forward.x * forwardDistance + right.x * lateralDistance;
	targetPosition.y -= 1.2f + std::sin(time * 1.6f) * 0.7f;
	targetPosition.z += forward.z * forwardDistance + right.z * lateralDistance;

	player.UpdateCamera(&camera, nullptr, false);
	camera.Update();
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const Vector3 screenPosition = MyMath::WorldToScreen(
		targetPosition, camera.GetViewProjectionMatrix(), screenWidth, screenHeight);
	if (screenPosition.z > 0.0f && screenPosition.z < 1.0f) {
		constexpr float kSafeTop = 0.35f;
		constexpr float kSafeBottom = 0.63f;
		const float desiredScreenY = std::clamp(screenPosition.y, screenHeight * kSafeTop, screenHeight * kSafeBottom);
		const float screenDelta = desiredScreenY - screenPosition.y;
		if (std::abs(screenDelta) > 1.0f) {
			const Vector3 cameraForward = NormalizeOr(Subtract(player.GetOBB().center, camera.GetTranslate()), forward);
			const Vector3 cameraRight = NormalizeOr(MyMath::Cross({ 0.0f, 1.0f, 0.0f }, cameraForward), right);
			const Vector3 cameraUp = NormalizeOr(MyMath::Cross(cameraForward, cameraRight), { 0.0f, 1.0f, 0.0f });
			const float worldOffset = std::clamp(screenDelta / screenHeight * 18.0f, -4.0f, 4.0f);
			targetPosition.x -= cameraUp.x * worldOffset;
			targetPosition.y -= cameraUp.y * worldOffset;
			targetPosition.z -= cameraUp.z * worldOffset;
		}
	}

	float currentBank = 0.0f;
	float currentYawOffset = 0.0f;
	float currentPitch = 0.0f;
	if (state.IsEntering()) {
		const float enterT = std::clamp(static_cast<float>(state.GetEnterFrame()) /
			static_cast<float>(TitleBackgroundEnemyState::kEnterDurationFrames), 0.0f, 1.0f);
		const float blend = std::pow(1.0f - enterT, 3.0f);
		float sideSign = static_cast<float>(state.GetSpawnSide() >= 0 ? 1 : -1);
		Vector3 candidatePosition = player.GetPosition();
		candidatePosition.x += right.x * (sideSign * 28.0f);
		candidatePosition.y += right.y * (sideSign * 28.0f);
		candidatePosition.z += right.z * (sideSign * 28.0f);
		if (const auto stageBoundsIt = std::find_if(obstacles.begin(), obstacles.end(),
			[](const std::unique_ptr<Obstacle>& obstacle) { return obstacle && obstacle->IsStageBounds(); });
			stageBoundsIt != obstacles.end()) {
			const Vector3 center = (*stageBoundsIt)->GetPosition();
			const Vector3 halfExtents = (*stageBoundsIt)->GetWorldHalfExtents();
			if (std::abs(candidatePosition.x - center.x) > halfExtents.x - 5.0f ||
				std::abs(candidatePosition.z - center.z) > halfExtents.z - 5.0f) {
				sideSign = -sideSign;
			}
		}

		targetPosition.x += right.x * (sideSign * 28.0f * blend) - forward.x * (12.0f * blend) + up.x * (3.5f * blend);
		targetPosition.y += right.y * (sideSign * 28.0f * blend) - forward.y * (12.0f * blend) + up.y * (3.5f * blend);
		targetPosition.z += right.z * (sideSign * 28.0f * blend) - forward.z * (12.0f * blend) + up.z * (3.5f * blend);
		currentBank = -sideSign * std::sin(enterT * 3.14159265f) * 0.70f;
		currentYawOffset = sideSign * (1.0f - enterT) * 0.45f;
		currentPitch = (1.0f - enterT) * 0.12f;
		state.AdvanceEntering();
	}

	if (const auto stageBoundsIt = std::find_if(obstacles.begin(), obstacles.end(),
		[](const std::unique_ptr<Obstacle>& obstacle) { return obstacle && obstacle->IsStageBounds(); });
		stageBoundsIt != obstacles.end()) {
		const Vector3 center = (*stageBoundsIt)->GetPosition();
		const Vector3 halfExtents = (*stageBoundsIt)->GetWorldHalfExtents();
		const Vector3 enemyHalfExtents = enemy.GetWorldHalfExtents();
		auto clampInsideStage = [](float value, float centerValue, float stageHalfExtent, float objectHalfExtent) {
			constexpr float kMargin = 1.5f;
			const float minimum = centerValue - stageHalfExtent + objectHalfExtent + kMargin;
			const float maximum = centerValue + stageHalfExtent - objectHalfExtent - kMargin;
			return minimum <= maximum ? std::clamp(value, minimum, maximum) : centerValue;
		};
		targetPosition.x = clampInsideStage(targetPosition.x, center.x, halfExtents.x, enemyHalfExtents.x);
		targetPosition.y = clampInsideStage(targetPosition.y, center.y, halfExtents.y, enemyHalfExtents.y);
		targetPosition.z = clampInsideStage(targetPosition.z, center.z, halfExtents.z, enemyHalfExtents.z);
	}

	enemy.SetPosition(targetPosition);
	const float baseYaw = std::atan2(-forward.x, -forward.z);
	enemy.SetRotation({ currentPitch, baseYaw + currentYawOffset, currentBank });
	enemy.UpdateModel();
}
