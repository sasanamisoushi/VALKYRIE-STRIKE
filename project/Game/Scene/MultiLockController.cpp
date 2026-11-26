#include "MultiLockController.h"

#include "GamePlaySceneHelpers.h"
#include "LockOnManager.h"
#include "MissilePresetManager.h"
#include "MultiLockChargeState.h"
#include "TargetSelectionState.h"
#include "TutorialPhaseController.h"
#include "TutorialProgressTracker.h"
#include "engine/math/MyMath.h"

#include <limits>

namespace {
// 最初の1件は押下直後にロックし、以降は長押し中に1件ずつ追加する。
constexpr int kMultiLockTargetAcquireIntervalFrames = 30;
}

bool MultiLockController::FindNextTarget(
	const LockOnSceneContext& context,
	Camera* activeCamera,
	MultiLockTarget& outTarget) const {
	if (!context.player || !activeCamera || context.player->IsDead()) {
		return false;
	}
	PlayerModeParams p = context.player->GetModeParams(context.player->GetCurrentMode());
	if (context.multiLockChargeState->GetTargetCount() >= static_cast<size_t>(p.maxMultiLock)) {
		return false;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
		return false;
	}

	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
		return false;
	}
	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.5f;

	const Vector3 playerPosition = context.player->GetPosition();
	const Vector3 playerForward = NormalizeOrVector3(context.player->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	float maxDistanceSq = (p.maxLockOnDistance * 2.0f) * (p.maxLockOnDistance * 2.0f);

	float bestScore = (std::numeric_limits<float>::max)();
	bool found = false;
	MultiLockTarget bestCandidate{};

	auto evaluateCandidate = [&](Enemy* enemy, int turretIndex, const Vector3& worldPos) {
		if (context.multiLockChargeState->HasTarget(enemy, turretIndex)) {
			return;
		}

		const Vector3 toTarget = SubtractVector3(worldPos, playerPosition);
		const float distSq = LengthSqVector3(toTarget);
		if (distSq > maxDistanceSq) {
			return;
		}

		const Vector3 direction = NormalizeOrVector3(toTarget, playerForward);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot < -0.3f) {
			return;
		}

		const Vector3 screenPos = MyMath::WorldToScreen(
			worldPos,
			activeCamera->GetViewProjectionMatrix(),
			screenWidth,
			screenHeight);

		if (screenPos.z < 0.0f || screenPos.z > 1.0f) {
			return;
		}

		const bool onScreen = (screenPos.x >= 0.0f && screenPos.x <= screenWidth &&
			screenPos.y >= 0.0f && screenPos.y <= screenHeight);

		const float dx = screenPos.x - centerX;
		const float dy = screenPos.y - centerY;
		float screenDistSq = dx * dx + dy * dy;

		if (!onScreen) {
			screenDistSq += screenWidth * screenWidth * 2.0f;
		}

		const float score = screenDistSq + distSq * 0.05f - forwardDot * 50.0f;
		if (score < bestScore) {
			bestScore = score;
			bestCandidate.enemy = enemy;
			bestCandidate.turretIndex = turretIndex;
			bestCandidate.targetPoint = worldPos;
			bestCandidate.hasTargetPoint = (turretIndex >= 0);
			found = true;
		}
	};

	for (const auto& enemy : (*context.enemies)) {
		if (!enemy || enemy->IsDead()) continue;

		const size_t partCount = enemy->GetLockOnPartCount();
		if (partCount > 0) {
			for (size_t i = 0; i < partCount; ++i) {
				if (enemy->IsLockOnPartAvailable(i)) {
					evaluateCandidate(enemy.get(), static_cast<int>(i), enemy->GetLockOnPartPosition(i));
				}
			}
			evaluateCandidate(enemy.get(), -1, enemy->GetBodyLockOnPosition());
		} else {
			evaluateCandidate(enemy.get(), -1, enemy->GetAimPosition());
		}
	}

	if (found) {
		outTarget = bestCandidate;
		return true;
	}
	return false;
}

void MultiLockController::Begin(const LockOnSceneContext& context) const {
	context.multiLockChargeState->BeginCharge();
}

void MultiLockController::PruneTargets(const LockOnSceneContext& context) const {
	auto isTargetAlive = [&context](const MultiLockTarget& target) {
		if (!target.enemy) {
			return false;
		}
		bool enemyFound = false;
		for (const auto& enemy : (*context.enemies)) {
			if (enemy.get() == target.enemy) {
				if (enemy->IsDead()) {
					return false;
				}
				enemyFound = true;
				break;
			}
		}
		if (!enemyFound) {
			return false;
		}

		if (target.turretIndex >= 0) {
			if (!target.enemy->IsLockOnPartAvailable(static_cast<size_t>(target.turretIndex))) {
				return false;
			}
		}
		return true;
	};

	context.multiLockChargeState->PruneTargetsIf([&](const MultiLockTarget& target) {
		return !isTargetAlive(target);
	});
}

void MultiLockController::Update(const LockOnSceneContext& context, Camera* activeCamera) const {
	if (!context.multiLockChargeState->IsCharging()) {
		return;
	}

	PruneTargets(context);

	// 既存ロックターゲットの位置を最新に更新
	context.multiLockChargeState->ForEachTargetMutable([](MultiLockTarget& target) {
		if (target.enemy) {
			if (target.turretIndex >= 0 && target.enemy->GetLockOnPartCount() > 0) {
				target.targetPoint = target.enemy->GetLockOnPartPosition(static_cast<size_t>(target.turretIndex));
				target.hasTargetPoint = true;
			} else {
				target.targetPoint = target.enemy->GetAimPosition();
			}
		}
	});

	PlayerModeParams p = context.player->GetModeParams(context.player->GetCurrentMode());
	if (context.multiLockChargeState->ShouldAcquireNextTarget(
			static_cast<size_t>(p.maxMultiLock), kMultiLockTargetAcquireIntervalFrames)) {
		MultiLockTarget nextTarget{};
		if (FindNextTarget(context, activeCamera, nextTarget)) {
			context.multiLockChargeState->AddTarget(nextTarget, true);
			if (nextTarget.enemy) {
				nextTarget.enemy->StartChasingPlayer();
			}
		}
	}

	context.multiLockChargeState->AdvanceChargeFrame();
}

void MultiLockController::FireMissiles(
	const LockOnSceneContext& context,
	bool isLockedEnemyAlive,
	const Vector3& lockedTargetPosition) const {
	if (!context.multiLockChargeState->IsCharging()) {
		return;
	}

	PruneTargets(context);
	if (!context.multiLockChargeState->HasTargets()) {
		if (Enemy* aimAssistEnemy = context.targetSelection->GetAimAssistEnemy()) {
			MultiLockTarget altTarget{};
			altTarget.enemy = aimAssistEnemy;
			altTarget.turretIndex = -1;
			altTarget.targetPoint = aimAssistEnemy->GetAimPosition();
			altTarget.hasTargetPoint = false;
			context.multiLockChargeState->AddTarget(altTarget);
		} else if (Enemy* lockedEnemy = context.targetSelection->GetLockedEnemy(); lockedEnemy && isLockedEnemyAlive) {
			const int turretIndex = context.targetSelection->GetLockedBossTurretIndex();
			MultiLockTarget altTarget{};
			altTarget.enemy = lockedEnemy;
			altTarget.turretIndex = turretIndex;
			altTarget.targetPoint = lockedTargetPosition;
			altTarget.hasTargetPoint = (turretIndex >= 0);
			context.multiLockChargeState->AddTarget(altTarget);
		}
	}

	const auto& targets = context.multiLockChargeState->GetTargets();
	if (targets.empty()) {
		if (context.missilePresetManager->FirePlayerMissile(MissileType::MissileWithTrail) &&
			context.tutorialController->IsEnabled() && context.tutorialController->GetPhase() == TutorialPhase::MultiLock) {
			context.tutorialProgress->RecordHomingLaunch();
		}
	} else {
		const float spacing = 0.35f;
		const float center = (static_cast<float>(targets.size()) - 1.0f) * 0.5f;
		for (size_t index = 0; index < targets.size(); ++index) {
			const float horizontalOffset = (static_cast<float>(index) - center) * spacing;
			const auto& target = targets[index];
			const Vector3* targetPointPtr = target.hasTargetPoint ? &target.targetPoint : nullptr;
			if (context.missilePresetManager->FirePlayerMissile(MissileType::MissileWithTrail,
				target.enemy, horizontalOffset, true, targetPointPtr) &&
				context.tutorialController->IsEnabled() && context.tutorialController->GetPhase() == TutorialPhase::MultiLock) {
				context.tutorialProgress->RecordHomingLaunch();
			}
		}
	}

	Cancel(context);
}

void MultiLockController::Cancel(const LockOnSceneContext& context) const {
	context.multiLockChargeState->CancelCharge();
}
