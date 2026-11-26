#include "LockOnManager.h"
#include "MissilePresetManager.h"
#include "MultiLockChargeState.h"
#include "MultiLockController.h"
#include "TargetSelectionState.h"
#include "TutorialPhaseController.h"
#include "TutorialProgressTracker.h"
#include "GamePlaySceneHelpers.h"
#include <externals/imgui/imgui.h>
#include <fstream>
#include <algorithm>
#include "engine/Input/Input.h"
#include "engine/math/MyMath.h"

namespace {
constexpr float kLockOnHoldDistanceMultiplier = 1.15f;
constexpr float kLockOnReleaseRearDot = -0.60f;
constexpr float kFighterLockSafetyMargin = 6.0f;
constexpr float kFighterLockWarningDistance = 36.0f;
constexpr int kFighterReacquireCooldownFrames = 45;
// 最初の1件は押下直後にロックし、以降は長押し中に1件ずつ追加する。
constexpr int kMultiLockTargetAcquireIntervalFrames = 30;
}

LockOnManager::LockOnManager(LockOnSceneContext context) : context_(std::move(context)) {}

bool LockOnManager::IsFighterReacquireBlocked(const Enemy *enemy) const {
	return fighterReacquireCooldownFrames_ > 0 && enemy == fighterRecentlyReleasedEnemy_;
}

void LockOnManager::ReleaseCurrentLock(bool preventImmediateReacquire) {
	if (preventImmediateReacquire && context_.targetSelection->HasLockedEnemy()) {
		fighterRecentlyReleasedEnemy_ = context_.targetSelection->GetLockedEnemy();
		fighterReacquireCooldownFrames_ = kFighterReacquireCooldownFrames;
	}
	context_.targetSelection->ClearAll();
	if (context_.resetCinematicLockOnCamera) { context_.resetCinematicLockOnCamera(); }
	isFighterLockDanger_ = false;
}

bool LockOnManager::ShouldKeepCurrentLock(Camera *activeCamera, bool &outTooClose) {
	outTooClose = false;
	isFighterLockDanger_ = false;
	if (!context_.player || !activeCamera || !context_.targetSelection->HasLockedEnemy() || context_.player->IsDead()) {
		return false;
	}

	const PlayerMode mode = context_.player->GetCurrentMode();
	const PlayerModeParams params = context_.player->GetModeParams(mode);
	const Vector3 toEnemy = SubtractVector3(GetLockedTargetPosition(), context_.player->GetPosition());
	const float distanceSq = LengthSqVector3(toEnemy);
	if (distanceSq <= 0.0001f) {
		outTooClose = (mode == PlayerMode::Fighter);
		return false;
	}

	float maxHoldDistance = params.maxLockOnDistance * kLockOnHoldDistanceMultiplier;
	if (IsPlayerJammed(activeCamera)) {
		maxHoldDistance *= 0.20f;
	}
	if (distanceSq > maxHoldDistance * maxHoldDistance) {
		return false;
	}

	const Vector3 playerForward = MyMath::Normalize(context_.player->GetForwardVector());
	const float forwardDot = MyMath::Dot(playerForward, MyMath::Normalize(toEnemy));
	if (forwardDot < kLockOnReleaseRearDot) {
		return false;
	}

	if (mode != PlayerMode::Fighter) {
		return true;
	}

	const float releaseDistance = context_.player->GetCollisionRadius()
		+ GetLockedTargetRadius()
		+ kFighterLockSafetyMargin;
	const float warningDistance = (std::max)(kFighterLockWarningDistance, releaseDistance + 12.0f);
	isFighterLockDanger_ = distanceSq <= warningDistance * warningDistance;
	outTooClose = distanceSq <= releaseDistance * releaseDistance;
	return !outTooClose;
}

void LockOnManager::UpdateLockOn(Camera *activeCamera, bool shouldUpdateGame) {
	Input *input = Input::GetInstance();
	if (!input) return;

	const bool isFighterMode = context_.player && context_.player->GetCurrentMode() == PlayerMode::Fighter;

	// タイトル画面のキー・マウス操作はメニュー専用。本編背景のロックオンへ渡さない。
	const bool canUsePlayerInput = !(context_.isTitleBackgroundMode && context_.isTitleBackgroundMode()) &&
		!IsImGuiKeyboardCaptureActive() && !IsImGuiMouseCaptureActive();
	if (shouldUpdateGame && fighterReacquireCooldownFrames_ > 0) {
		--fighterReacquireCooldownFrames_;
		if (fighterReacquireCooldownFrames_ == 0) {
			fighterRecentlyReleasedEnemy_ = nullptr;
		}
	}

	if (isFighterMode) {
		// ファイターモードでは手動固定ロックオン(lockedEnemy_)は行わない
		if (context_.targetSelection->HasLockedEnemy()) {
			ReleaseCurrentLock(false);
		}
		// 機体前方の敵に対するエイムアシストおよびマルチロックは全モード共通で動作させる
		context_.targetSelection->ClearAimAssist();
		if (shouldUpdateGame) {
			context_.targetSelection->SetAimAssistEnemy(FindAimAssistTarget(activeCamera));
		}
		return;
	}

	if (!IsLockedEnemyAlive()) {
		if (context_.targetSelection->HasLockedBossTurret()) {
			Enemy* lockedEnemy = context_.targetSelection->GetLockedEnemy();
			if (lockedEnemy && lockedEnemy->GetLockOnPartCount() > 0 && !lockedEnemy->IsDead()) {
				SelectBossTurretForLock(activeCamera);
			} else {
				ReleaseCurrentLock(false);
			}
		} else {
			ReleaseCurrentLock(false);
		}
	} else if (shouldUpdateGame) {
		bool tooClose = false;
		if (!ShouldKeepCurrentLock(activeCamera, tooClose)) {
			ReleaseCurrentLock(tooClose);
		}
	}

	if (canUsePlayerInput && input->TriggerAction(PlayerAction::LockToggle)) {
		// ロック中なら対象（別の敵や別の砲台・部位）へ切り替え。未ロックなら対象を探してロック。
		if (context_.targetSelection->HasLockedEnemy() && IsLockedEnemyAlive()) {
			SwitchLockOnTarget(activeCamera);
		} else {
			context_.targetSelection->SetLockedTarget(FindLockOnTarget(activeCamera), -1);
			SelectBossTurretForLock(activeCamera);
		}
		context_.targetSelection->ClearAimAssist();
		if (context_.resetCinematicLockOnCamera) { context_.resetCinematicLockOnCamera(); }
	}

	context_.targetSelection->ClearAimAssist();
	if (!context_.targetSelection->HasLockedEnemy() && shouldUpdateGame) {
		context_.targetSelection->SetAimAssistEnemy(FindAimAssistTarget(activeCamera));
	}

	if (!shouldUpdateGame) {
		return;
	}

	if (canUsePlayerInput && input->TriggerAction(PlayerAction::LockRelease)) {
		ReleaseCurrentLock(false);
		return;
	}

	if (Enemy* lockedEnemy = context_.targetSelection->GetLockedEnemy()) {
		lockedEnemy->StartChasingPlayer();
	}
}

void LockOnManager::SwitchLockOnTarget(Camera *activeCamera) {
	if (!context_.player || !activeCamera || context_.player->IsDead()) {
		ReleaseCurrentLock(false);
		return;
	}

	PlayerModeParams p = context_.player->GetModeParams(context_.player->GetCurrentMode());
	const Vector3 playerPosition = context_.player->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(context_.player->GetForwardVector());

	float maxDistSq = p.maxLockOnDistance * p.maxLockOnDistance;
	const bool isJammed = IsPlayerJammed(activeCamera);
	if (isJammed) {
		maxDistSq *= 0.04f;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
		ReleaseCurrentLock(false);
		return;
	}
	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
		ReleaseCurrentLock(false);
		return;
	}

	struct Candidate {
		Enemy *enemy = nullptr;
		int turretIndex = -1;
		Vector3 worldPos{};
		Vector3 screenPos{};
	};

	std::vector<Candidate> candidates;

	auto addCandidateIfValid = [&](Enemy *enemy, int turretIndex, const Vector3 &worldPos) {
		const Vector3 toTarget = SubtractVector3(worldPos, playerPosition);
		const float distSq = LengthSqVector3(toTarget);
		if (distSq > maxDistSq) {
			return;
		}

		if (context_.player->GetCurrentMode() == PlayerMode::Fighter) {
			const float minimumDistance = context_.player->GetCollisionRadius()
				+ enemy->GetCollisionRadius() + kFighterLockSafetyMargin;
			if (distSq <= minimumDistance * minimumDistance) return;
		}

		const Vector3 direction = MyMath::Normalize(toTarget);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot < p.lockOnAngleDot - 0.25f) {
			return;
		}

		Vector3 screenPos = MyMath::WorldToScreen(
			worldPos,
			activeCamera->GetViewProjectionMatrix(),
			screenWidth,
			screenHeight);

		if (screenPos.z < 0.0f || screenPos.z > 1.0f) {
			return;
		}

		if (screenPos.x < -20.0f || screenPos.x > screenWidth + 20.0f ||
			screenPos.y < -20.0f || screenPos.y > screenHeight + 20.0f) {
			return;
		}

		candidates.push_back({ enemy, turretIndex, worldPos, screenPos });
	};

	for (const auto &enemy : (*context_.enemies)) {
		if (!enemy || enemy->IsDead()) continue;
		if (IsFighterReacquireBlocked(enemy.get())) continue;

		const size_t partCount = enemy->GetLockOnPartCount();
		if (partCount > 0) {
			for (size_t i = 0; i < partCount; ++i) {
				if (enemy->IsLockOnPartAvailable(i)) {
					addCandidateIfValid(enemy.get(), static_cast<int>(i), enemy->GetLockOnPartPosition(i));
				}
			}
			addCandidateIfValid(enemy.get(), -1, enemy->GetBodyLockOnPosition());
		} else {
			addCandidateIfValid(enemy.get(), -1, enemy->GetAimPosition());
		}
	}

	if (candidates.empty()) {
		ReleaseCurrentLock(false);
		return;
	}

	// 画面のX座標（左から右へ）でソート
	std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
		return a.screenPos.x < b.screenPos.x;
	});

	// 現在のロック対象のインデックスを特定
	int currentIndex = -1;
	for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
		if (candidates[i].enemy == context_.targetSelection->GetLockedEnemy() &&
			candidates[i].turretIndex == context_.targetSelection->GetLockedBossTurretIndex()) {
			currentIndex = i;
			break;
		}
	}

	if (currentIndex >= 0) {
		if (candidates.size() == 1) {
			// 対象が1つしかない場合はロック解除（トグル）
			ReleaseCurrentLock(false);
			return;
		}
		// 次の対象へ切り替え
		int nextIndex = (currentIndex + 1) % static_cast<int>(candidates.size());
		context_.targetSelection->SetLockedTarget(candidates[nextIndex].enemy, candidates[nextIndex].turretIndex);
	} else {
		// 画面中央に最も近い対象を選択
		const float centerX = screenWidth * 0.5f;
		const float centerY = screenHeight * 0.5f;
		int bestIdx = 0;
		float bestDistSq = (std::numeric_limits<float>::max)();
		for (int i = 0; i < static_cast<int>(candidates.size()); ++i) {
			float dx = candidates[i].screenPos.x - centerX;
			float dy = candidates[i].screenPos.y - centerY;
			float d2 = dx * dx + dy * dy;
			if (d2 < bestDistSq) {
				bestDistSq = d2;
				bestIdx = i;
			}
		}
		context_.targetSelection->SetLockedTarget(candidates[bestIdx].enemy, candidates[bestIdx].turretIndex);
	}

	if (Enemy* lockedEnemy = context_.targetSelection->GetLockedEnemy()) {
		lockedEnemy->StartChasingPlayer();
	}
	context_.targetSelection->ClearAimAssist();
	if (context_.resetCinematicLockOnCamera) { context_.resetCinematicLockOnCamera(); }
}

Enemy *LockOnManager::FindLockOnTarget(Camera *activeCamera) {
	if (!context_.player || !activeCamera || context_.player->IsDead()) {
		return nullptr;
	}
	if (context_.player->GetCurrentMode() == PlayerMode::Fighter) {
		return nullptr;
	}

	const Vector3 playerPosition = context_.player->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(context_.player->GetForwardVector());
	Enemy *nearestAliveEnemy = nullptr;
	float nearestAliveDistSq = (std::numeric_limits<float>::max)();
	Enemy *bestFrontEnemy = nullptr;
	float bestFrontScore = (std::numeric_limits<float>::max)();
	Enemy *bestScreenEnemy = nullptr;
	float bestScreenScore = (std::numeric_limits<float>::max)();

	PlayerModeParams p = context_.player->GetModeParams(context_.player->GetCurrentMode());
	float maxDistSq = p.maxLockOnDistance * p.maxLockOnDistance;
	
	const bool isJammed = IsPlayerJammed(activeCamera);
	const float maxJammedDistanceSq = maxDistSq * 0.04f;
	const float effectiveMaxDistSq = isJammed ? maxJammedDistanceSq : maxDistSq;

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	const bool hasOverlayBounds = activeCamera && GetOverlayBounds(minX, minY, maxX, maxY);
	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;

	for (const auto &enemy : (*context_.enemies)) {
		if (!enemy || enemy->IsDead()) continue;
		if (IsFighterReacquireBlocked(enemy.get())) continue;

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq > effectiveMaxDistSq) {
			continue;
		}
		if (context_.player->GetCurrentMode() == PlayerMode::Fighter) {
			const float minimumDistance = context_.player->GetCollisionRadius()
				+ enemy->GetCollisionRadius() + kFighterLockSafetyMargin;
			if (distSq <= minimumDistance * minimumDistance) continue;
		}

		if (distSq < nearestAliveDistSq) {
			nearestAliveDistSq = distSq;
			nearestAliveEnemy = enemy.get();
		}

		const Vector3 direction = MyMath::Normalize(toEnemy);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot >= p.lockOnAngleDot) {
			const float frontScore = distSq * 0.01f - forwardDot * 1000.0f;
			if (frontScore < bestFrontScore) {
				bestFrontScore = frontScore;
				bestFrontEnemy = enemy.get();
			}
		}

		if (hasOverlayBounds && screenWidth > 0.0f && screenHeight > 0.0f && forwardDot >= p.lockOnAngleDot - 0.2f) {
			Vector3 targetPosition = enemy->GetAimPosition();
			const float collisionRadius = enemy->GetCollisionRadius();
			if (!enemy->IsBoss()) targetPosition.y += collisionRadius * 0.3f;
			Vector3 screenPosition = MyMath::WorldToScreen(
				targetPosition,
				activeCamera->GetViewProjectionMatrix(),
				screenWidth,
				screenHeight);

			if (screenPosition.z >= 0.0f && screenPosition.z <= 1.0f &&
				screenPosition.x >= 0.0f && screenPosition.x <= screenWidth &&
				screenPosition.y >= 0.0f && screenPosition.y <= screenHeight) {
				const float normalizedX = (screenPosition.x - screenWidth * 0.5f) / (screenWidth * 0.5f);
				const float normalizedY = (screenPosition.y - screenHeight * 0.5f) / (screenHeight * 0.5f);
				const float centerScore = normalizedX * normalizedX + normalizedY * normalizedY;
				const float screenScore = centerScore * 10000.0f + distSq * 0.002f - forwardDot * 150.0f;
				if (screenScore < bestScreenScore) {
					bestScreenScore = screenScore;
					bestScreenEnemy = enemy.get();
				}
			}
		}
	}

	if (bestScreenEnemy) {
		return bestScreenEnemy;
	}
	if (bestFrontEnemy) {
		return bestFrontEnemy;
	}
	return nearestAliveEnemy;
}

bool LockOnManager::IsLockedEnemyAlive() const {
	Enemy* lockedEnemy = context_.targetSelection->GetLockedEnemy();
	if (!lockedEnemy) {
		return false;
	}

	const int turretIndex = context_.targetSelection->GetLockedBossTurretIndex();
	if (turretIndex >= 0) {
		return lockedEnemy->IsLockOnPartAvailable(static_cast<size_t>(turretIndex));
	}

	for (const auto &enemy : (*context_.enemies)) {
		if (enemy.get() == lockedEnemy) {
			return !enemy->IsDead();
		}
	}

	return false;
}

void LockOnManager::SelectBossTurretForLock(Camera *activeCamera) {
	context_.targetSelection->SetLockedBossTurretIndex(-1);
	Enemy* lockedEnemy = context_.targetSelection->GetLockedEnemy();
	if (!lockedEnemy || lockedEnemy->GetLockOnPartCount() == 0 || !activeCamera) return;

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) return;
	const float width = maxX - minX;
	const float height = maxY - minY;
	if (width <= 0.0f || height <= 0.0f) return;

	float bestScore = (std::numeric_limits<float>::max)();
	for (size_t index = 0; index < lockedEnemy->GetLockOnPartCount(); ++index) {
		if (!lockedEnemy->IsLockOnPartAvailable(index)) continue;
		const Vector3 screenPosition = MyMath::WorldToScreen(lockedEnemy->GetLockOnPartPosition(index),
			activeCamera->GetViewProjectionMatrix(), width, height);
		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
			screenPosition.x < 0.0f || screenPosition.x > width ||
			screenPosition.y < 0.0f || screenPosition.y > height) continue;
		const float dx = screenPosition.x - width * 0.5f;
		const float dy = screenPosition.y - height * 0.5f;
		const float score = dx * dx + dy * dy;
		if (score < bestScore) {
			bestScore = score;
			context_.targetSelection->SetLockedBossTurretIndex(static_cast<int>(index));
		}
	}
}

bool LockOnManager::HasLockedBossTurret() const {
	return context_.targetSelection->HasLockedBossTurret() && IsLockedEnemyAlive();
}

Vector3 LockOnManager::GetLockedTargetPosition() const {
	const Enemy* lockedEnemy = context_.targetSelection->GetLockedEnemy();
	if (!lockedEnemy) {
		return Vector3{};
	}
	if (HasLockedBossTurret()) {
		return lockedEnemy->GetLockOnPartPosition(static_cast<size_t>(context_.targetSelection->GetLockedBossTurretIndex()));
	}
	return lockedEnemy->GetBodyLockOnPosition();
}

float LockOnManager::GetLockedTargetRadius() const {
	const Enemy* lockedEnemy = context_.targetSelection->GetLockedEnemy();
	if (!lockedEnemy) {
		return 1.0f;
	}
	if (HasLockedBossTurret()) {
		return lockedEnemy->GetLockOnPartRadius(static_cast<size_t>(context_.targetSelection->GetLockedBossTurretIndex()));
	}
	return lockedEnemy->GetCollisionRadius();
}

int LockOnManager::GetLockedBossTurretIndex() const {
	return HasLockedBossTurret() ? context_.targetSelection->GetLockedBossTurretIndex() : -1;
}

bool LockOnManager::IsPlayerJammed(Camera* activeCamera) const {
	if (!context_.player || context_.player->IsDead()) return false;
	const Vector3 playerPos = context_.player->GetPosition();
	for (const auto& enemy : (*context_.enemies)) {
		if (enemy && !enemy->IsDead()) {
			const float jammerRadius = enemy->GetJammingRadius();
			if (jammerRadius > 0.0f) {
				const float distSq = LengthSqVector3(SubtractVector3(playerPos, enemy->GetPosition()));
				if (distSq <= jammerRadius * jammerRadius) {
					return true;
				}
			}
		}
	}
	return false;
}

Enemy *LockOnManager::FindAimAssistTarget(Camera *activeCamera) {
	if (!context_.player || !activeCamera || context_.player->IsDead()) {
		return nullptr;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
		return nullptr;
	}

	const float screenWidth = maxX - minX;
	const float screenHeight = maxY - minY;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) {
		return nullptr;
	}

	PlayerModeParams p = context_.player->GetModeParams(context_.player->GetCurrentMode());
	const Vector3 playerPosition = context_.player->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(context_.player->GetForwardVector());
	float maxDistanceSq = p.maxLockOnDistance * p.maxLockOnDistance;
	if (IsPlayerJammed(activeCamera)) {
		maxDistanceSq *= 0.04f; // 距離を20%に制限 (0.2 * 0.2 = 0.04)
	}
	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.5f;
	Enemy *bestTarget = nullptr;
	float bestScore = (std::numeric_limits<float>::max)();

	for (const auto &enemy : (*context_.enemies)) {
		if (!enemy || enemy->IsDead()) continue;
		if (IsFighterReacquireBlocked(enemy.get())) continue;

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq > maxDistanceSq) {
			continue;
		}
		if (context_.player->GetCurrentMode() == PlayerMode::Fighter) {
			const float minimumDistance = context_.player->GetCollisionRadius()
				+ enemy->GetCollisionRadius() + kFighterLockSafetyMargin;
			if (distSq <= minimumDistance * minimumDistance) continue;
		}

		const Vector3 direction = MyMath::Normalize(toEnemy);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot < (std::min)(p.lockOnAngleDot, -0.3f)) {
			continue;
		}

		Vector3 targetPosition = enemy->GetAimPosition();
		const float collisionRadius = enemy->GetCollisionRadius();
		if (!enemy->IsBoss()) targetPosition.y += collisionRadius * 0.3f;

		Vector3 screenPosition = MyMath::WorldToScreen(
			targetPosition,
			activeCamera->GetViewProjectionMatrix(),
			screenWidth,
			screenHeight);

		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
			screenPosition.x < 0.0f || screenPosition.x > screenWidth ||
			screenPosition.y < 0.0f || screenPosition.y > screenHeight) {
			continue;
		}

		const float dx = screenPosition.x - centerX;
		const float dy = screenPosition.y - centerY;
		const float screenDistanceSq = dx * dx + dy * dy;
		const float assistRadius = kAimAssistScreenRadius + collisionRadius * 12.0f;
		if (screenDistanceSq > assistRadius * assistRadius) {
			continue;
		}

		const float score = screenDistanceSq + distSq * 0.01f - forwardDot * 40.0f;
		if (score < bestScore) {
			bestScore = score;
			bestTarget = enemy.get();
		}
	}

	return bestTarget;
}

bool LockOnManager::FindNextMultiLockTarget(Camera *activeCamera, MultiLockTarget &outTarget) const {
	return MultiLockController{}.FindNextTarget(context_, activeCamera, outTarget);
}

void LockOnManager::BeginMultiLock() {
	MultiLockController{}.Begin(context_);
}

void LockOnManager::PruneMultiLockTargets() {
	MultiLockController{}.PruneTargets(context_);
}

void LockOnManager::UpdateMultiLock(Camera *activeCamera) {
	MultiLockController{}.Update(context_, activeCamera);
}

void LockOnManager::FireMultiLockMissiles() {
	MultiLockController{}.FireMissiles(context_, IsLockedEnemyAlive(), GetLockedTargetPosition());
}

void LockOnManager::CancelMultiLock() {
	MultiLockController{}.Cancel(context_);
}
