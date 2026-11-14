#include "LockOnManager.h"
#include "MissilePresetManager.h"
#include "GamePlayScene.h"
#include "GamePlaySceneHelpers.h"
#include "Game/enemy/JammerEnemy.h"
#include "Game/enemy/Boss.h"
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

LockOnManager::LockOnManager(GamePlayScene* scene) : scene_(scene) {}

bool LockOnManager::IsFighterReacquireBlocked(const Enemy *enemy) const {
	return fighterReacquireCooldownFrames_ > 0 && enemy == fighterRecentlyReleasedEnemy_;
}

void LockOnManager::ReleaseCurrentLock(bool preventImmediateReacquire) {
	if (preventImmediateReacquire && scene_->lockedEnemy_) {
		fighterRecentlyReleasedEnemy_ = scene_->lockedEnemy_;
		fighterReacquireCooldownFrames_ = kFighterReacquireCooldownFrames;
	}
	scene_->lockedEnemy_ = nullptr;
	scene_->lockedBossTurretIndex_ = -1;
	scene_->aimAssistEnemy_ = nullptr;
	scene_->isCinematicLockOnCameraInitialized_ = false;
	isFighterLockDanger_ = false;
}

bool LockOnManager::ShouldKeepCurrentLock(Camera *activeCamera, bool &outTooClose) {
	outTooClose = false;
	isFighterLockDanger_ = false;
	if (!scene_->player_ || !activeCamera || !scene_->lockedEnemy_ || scene_->player_->IsDead()) {
		return false;
	}

	const PlayerMode mode = scene_->player_->GetCurrentMode();
	const PlayerModeParams params = scene_->player_->GetModeParams(mode);
	const Vector3 toEnemy = SubtractVector3(GetLockedTargetPosition(), scene_->player_->GetPosition());
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

	const Vector3 playerForward = MyMath::Normalize(scene_->player_->GetForwardVector());
	const float forwardDot = MyMath::Dot(playerForward, MyMath::Normalize(toEnemy));
	if (forwardDot < kLockOnReleaseRearDot) {
		return false;
	}

	if (mode != PlayerMode::Fighter) {
		return true;
	}

	const float releaseDistance = scene_->player_->GetCollisionRadius()
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

	const bool isFighterMode = scene_->player_ && scene_->player_->GetCurrentMode() == PlayerMode::Fighter;

	// タイトル画面のキー・マウス操作はメニュー専用。本編背景のロックオンへ渡さない。
	const bool canUsePlayerInput = !scene_->IsTitleBackgroundMode() &&
		!IsImGuiKeyboardCaptureActive() && !IsImGuiMouseCaptureActive();
	if (shouldUpdateGame && fighterReacquireCooldownFrames_ > 0) {
		--fighterReacquireCooldownFrames_;
		if (fighterReacquireCooldownFrames_ == 0) {
			fighterRecentlyReleasedEnemy_ = nullptr;
		}
	}

	if (isFighterMode) {
		// ファイターモードでは手動固定ロックオン(lockedEnemy_)は行わない
		if (scene_->lockedEnemy_) {
			ReleaseCurrentLock(false);
		}
		// 機体前方の敵に対するエイムアシストおよびマルチロックは全モード共通で動作させる
		scene_->aimAssistEnemy_ = nullptr;
		if (shouldUpdateGame) {
			scene_->aimAssistEnemy_ = FindAimAssistTarget(activeCamera);
		}
		return;
	}

	if (!IsLockedEnemyAlive()) {
		if (scene_->lockedBossTurretIndex_ >= 0) {
			Boss* boss = dynamic_cast<Boss*>(scene_->lockedEnemy_);
			if (boss && !boss->IsDead()) {
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
		if (scene_->lockedEnemy_ && IsLockedEnemyAlive()) {
			SwitchLockOnTarget(activeCamera);
		} else {
			scene_->lockedEnemy_ = FindLockOnTarget(activeCamera);
			SelectBossTurretForLock(activeCamera);
		}
		scene_->aimAssistEnemy_ = nullptr;
		scene_->isCinematicLockOnCameraInitialized_ = false;
	}

	scene_->aimAssistEnemy_ = nullptr;
	if (!scene_->lockedEnemy_ && shouldUpdateGame) {
		scene_->aimAssistEnemy_ = FindAimAssistTarget(activeCamera);
	}

	if (!shouldUpdateGame) {
		return;
	}

	if (canUsePlayerInput && input->TriggerAction(PlayerAction::LockRelease)) {
		ReleaseCurrentLock(false);
		return;
	}

	if (scene_->lockedEnemy_) {
		scene_->lockedEnemy_->StartChasingPlayer();
	}
}

void LockOnManager::SwitchLockOnTarget(Camera *activeCamera) {
	if (!scene_->player_ || !activeCamera || scene_->player_->IsDead()) {
		ReleaseCurrentLock(false);
		return;
	}

	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
	const Vector3 playerPosition = scene_->player_->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(scene_->player_->GetForwardVector());

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

		if (scene_->player_->GetCurrentMode() == PlayerMode::Fighter) {
			const float minimumDistance = scene_->player_->GetCollisionRadius()
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

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }
		if (IsFighterReacquireBlocked(enemy.get())) continue;

		if (Boss *boss = dynamic_cast<Boss*>(enemy.get())) {
			for (size_t i = 0; i < boss->GetTurretCount(); ++i) {
				if (boss->IsTurretAlive(i)) {
					addCandidateIfValid(boss, static_cast<int>(i), boss->GetTurretLockOnPosition(i));
				}
			}
			addCandidateIfValid(boss, -1, boss->GetBodyLockOnPosition());
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
		if (candidates[i].enemy == scene_->lockedEnemy_ &&
			candidates[i].turretIndex == scene_->lockedBossTurretIndex_) {
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
		scene_->lockedEnemy_ = candidates[nextIndex].enemy;
		scene_->lockedBossTurretIndex_ = candidates[nextIndex].turretIndex;
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
		scene_->lockedEnemy_ = candidates[bestIdx].enemy;
		scene_->lockedBossTurretIndex_ = candidates[bestIdx].turretIndex;
	}

	if (scene_->lockedEnemy_) {
		scene_->lockedEnemy_->StartChasingPlayer();
	}
	scene_->aimAssistEnemy_ = nullptr;
	scene_->isCinematicLockOnCameraInitialized_ = false;
}

Enemy *LockOnManager::FindLockOnTarget(Camera *activeCamera) {
	if (!scene_->player_ || !activeCamera || scene_->player_->IsDead()) {
		return nullptr;
	}
	if (scene_->player_->GetCurrentMode() == PlayerMode::Fighter) {
		return nullptr;
	}

	const Vector3 playerPosition = scene_->player_->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(scene_->player_->GetForwardVector());
	Enemy *nearestAliveEnemy = nullptr;
	float nearestAliveDistSq = (std::numeric_limits<float>::max)();
	Enemy *bestFrontEnemy = nullptr;
	float bestFrontScore = (std::numeric_limits<float>::max)();
	Enemy *bestScreenEnemy = nullptr;
	float bestScreenScore = (std::numeric_limits<float>::max)();

	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
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

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }
		if (IsFighterReacquireBlocked(enemy.get())) continue;

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq > effectiveMaxDistSq) {
			continue;
		}
		if (scene_->player_->GetCurrentMode() == PlayerMode::Fighter) {
			const float minimumDistance = scene_->player_->GetCollisionRadius()
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
			float collisionRadius = 1.0f;
			try {
				collisionRadius = enemy->GetCollisionRadius();
			} catch (...) {}
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
	if (!scene_->lockedEnemy_) {
		return false;
	}

	if (scene_->lockedBossTurretIndex_ >= 0) {
		const Boss* boss = dynamic_cast<const Boss*>(scene_->lockedEnemy_);
		return boss && boss->IsTurretAlive(static_cast<size_t>(scene_->lockedBossTurretIndex_));
	}

	for (const auto &enemy : scene_->enemies_) {
		if (enemy.get() == scene_->lockedEnemy_) {
			try {
				if (!enemy->IsDead()) {
					return true;
				}
			} catch (...) {
				return false;
			}
		}
	}

	return false;
}

void LockOnManager::SelectBossTurretForLock(Camera *activeCamera) {
	scene_->lockedBossTurretIndex_ = -1;
	Boss* boss = dynamic_cast<Boss*>(scene_->lockedEnemy_);
	if (!boss || !activeCamera) return;

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetOverlayBounds(minX, minY, maxX, maxY)) return;
	const float width = maxX - minX;
	const float height = maxY - minY;
	if (width <= 0.0f || height <= 0.0f) return;

	float bestScore = (std::numeric_limits<float>::max)();
	for (size_t index = 0; index < boss->GetTurretCount(); ++index) {
		if (!boss->IsTurretAlive(index)) continue;
		const Vector3 screenPosition = MyMath::WorldToScreen(boss->GetTurretLockOnPosition(index),
			activeCamera->GetViewProjectionMatrix(), width, height);
		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
			screenPosition.x < 0.0f || screenPosition.x > width ||
			screenPosition.y < 0.0f || screenPosition.y > height) continue;
		const float dx = screenPosition.x - width * 0.5f;
		const float dy = screenPosition.y - height * 0.5f;
		const float score = dx * dx + dy * dy;
		if (score < bestScore) {
			bestScore = score;
			scene_->lockedBossTurretIndex_ = static_cast<int>(index);
		}
	}
}

bool LockOnManager::HasLockedBossTurret() const {
	return scene_->lockedBossTurretIndex_ >= 0 && IsLockedEnemyAlive();
}

Vector3 LockOnManager::GetLockedTargetPosition() const {
	if (HasLockedBossTurret()) {
		const Boss* boss = static_cast<const Boss*>(scene_->lockedEnemy_);
		return boss->GetTurretLockOnPosition(static_cast<size_t>(scene_->lockedBossTurretIndex_));
	}
	if (const Boss* boss = dynamic_cast<const Boss*>(scene_->lockedEnemy_)) {
		return boss->GetBodyLockOnPosition();
	}
	return scene_->lockedEnemy_ ? scene_->lockedEnemy_->GetPosition() : Vector3{};
}

float LockOnManager::GetLockedTargetRadius() const {
	if (HasLockedBossTurret()) {
		const Boss* boss = static_cast<const Boss*>(scene_->lockedEnemy_);
		return boss->GetTurretLockOnRadius(static_cast<size_t>(scene_->lockedBossTurretIndex_));
	}
	return scene_->lockedEnemy_ ? scene_->lockedEnemy_->GetCollisionRadius() : 1.0f;
}

int LockOnManager::GetLockedBossTurretIndex() const {
	return HasLockedBossTurret() ? scene_->lockedBossTurretIndex_ : -1;
}

bool LockOnManager::IsPlayerJammed(Camera* activeCamera) const {
	if (!scene_->player_ || scene_->player_->IsDead()) return false;
	const Vector3 playerPos = scene_->player_->GetPosition();
	for (const auto& enemy : scene_->enemies_) {
		if (enemy && !enemy->IsDead()) {
			if (auto jammer = dynamic_cast<JammerEnemy*>(enemy.get())) {
				float distSq = LengthSqVector3(SubtractVector3(playerPos, jammer->GetPosition()));
				float jammerRadius = jammer->GetJammingRadius();
				if (distSq <= jammerRadius * jammerRadius) {
					return true;
				}
			}
		}
	}
	return false;
}

Enemy *LockOnManager::FindAimAssistTarget(Camera *activeCamera) {
	if (!scene_->player_ || !activeCamera || scene_->player_->IsDead()) {
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

	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
	const Vector3 playerPosition = scene_->player_->GetPosition();
	const Vector3 playerForward = MyMath::Normalize(scene_->player_->GetForwardVector());
	float maxDistanceSq = p.maxLockOnDistance * p.maxLockOnDistance;
	if (IsPlayerJammed(activeCamera)) {
		maxDistanceSq *= 0.04f; // 距離を20%に制限 (0.2 * 0.2 = 0.04)
	}
	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.5f;
	Enemy *bestTarget = nullptr;
	float bestScore = (std::numeric_limits<float>::max)();

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }
		if (IsFighterReacquireBlocked(enemy.get())) continue;

		const Vector3 toEnemy = SubtractVector3(enemy->GetPosition(), playerPosition);
		const float distSq = LengthSqVector3(toEnemy);
		if (distSq > maxDistanceSq) {
			continue;
		}
		if (scene_->player_->GetCurrentMode() == PlayerMode::Fighter) {
			const float minimumDistance = scene_->player_->GetCollisionRadius()
				+ enemy->GetCollisionRadius() + kFighterLockSafetyMargin;
			if (distSq <= minimumDistance * minimumDistance) continue;
		}

		const Vector3 direction = MyMath::Normalize(toEnemy);
		const float forwardDot = MyMath::Dot(playerForward, direction);
		if (forwardDot < (std::min)(p.lockOnAngleDot, -0.3f)) {
			continue;
		}

		Vector3 targetPosition = enemy->GetAimPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = enemy->GetCollisionRadius();
		} catch (...) {}
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
	if (!scene_->player_ || !activeCamera || scene_->player_->IsDead()) {
		return false;
	}
	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
	if (scene_->multiLockTargets_.size() >= p.maxMultiLock) {
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

	const Vector3 playerPosition = scene_->player_->GetPosition();
	const Vector3 playerForward = NormalizeOrVector3(scene_->player_->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	float maxDistanceSq = (p.maxLockOnDistance * 2.0f) * (p.maxLockOnDistance * 2.0f);

	float bestScore = (std::numeric_limits<float>::max)();
	bool found = false;
	MultiLockTarget bestCandidate{};

	auto evaluateCandidate = [&](Enemy *enemy, int turretIndex, const Vector3 &worldPos) {
		for (const auto &locked : scene_->multiLockTargets_) {
			if (locked.enemy == enemy && locked.turretIndex == turretIndex) {
				return;
			}
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

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy.get()) continue;
		try {
			if (enemy->IsDead()) continue;
		} catch (...) { continue; }

		if (Boss *boss = dynamic_cast<Boss*>(enemy.get())) {
			for (size_t i = 0; i < boss->GetTurretCount(); ++i) {
				if (boss->IsTurretAlive(i)) {
					evaluateCandidate(boss, static_cast<int>(i), boss->GetTurretLockOnPosition(i));
				}
			}
			evaluateCandidate(boss, -1, boss->GetBodyLockOnPosition());
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

void LockOnManager::BeginMultiLock() {
	scene_->isMultiLockCharging_ = true;
	scene_->multiLockChargeFrames_ = 0;
	scene_->multiLockTargets_.clear();
}

void LockOnManager::PruneMultiLockTargets() {
	auto isTargetAlive = [this](const MultiLockTarget &target) {
		if (!target.enemy) {
			return false;
		}
		bool enemyFound = false;
		for (const auto &enemy : scene_->enemies_) {
			if (enemy.get() == target.enemy) {
				try {
					if (enemy->IsDead()) {
						return false;
					}
				} catch (...) {
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
			if (const Boss *boss = dynamic_cast<const Boss*>(target.enemy)) {
				if (!boss->IsTurretAlive(static_cast<size_t>(target.turretIndex))) {
					return false;
				}
			}
		}
		return true;
	};

	scene_->multiLockTargets_.erase(
		std::remove_if(
			scene_->multiLockTargets_.begin(),
			scene_->multiLockTargets_.end(),
			[&](const MultiLockTarget &target) { return !isTargetAlive(target); }),
		scene_->multiLockTargets_.end());
}

void LockOnManager::UpdateMultiLock(Camera *activeCamera) {
	if (!scene_->isMultiLockCharging_) {
		return;
	}

	PruneMultiLockTargets();

	// 既存ロックターゲットの位置を最新に更新
	for (auto &target : scene_->multiLockTargets_) {
		if (target.enemy) {
			if (target.turretIndex >= 0) {
				if (const Boss *boss = dynamic_cast<const Boss*>(target.enemy)) {
					target.targetPoint = boss->GetTurretLockOnPosition(static_cast<size_t>(target.turretIndex));
					target.hasTargetPoint = true;
				}
			} else {
				target.targetPoint = target.enemy->GetAimPosition();
			}
		}
	}

	PlayerModeParams p = scene_->player_->GetModeParams(scene_->player_->GetCurrentMode());
	const bool shouldAcquireTarget = scene_->multiLockTargets_.empty() ||
		scene_->multiLockChargeFrames_ >= kMultiLockTargetAcquireIntervalFrames;
	if (scene_->multiLockTargets_.size() < p.maxMultiLock && shouldAcquireTarget) {
		MultiLockTarget nextTarget{};
		if (FindNextMultiLockTarget(activeCamera, nextTarget)) {
			scene_->multiLockTargets_.push_back(nextTarget);
			scene_->multiLockChargeFrames_ = 0;
			if (nextTarget.enemy) {
				nextTarget.enemy->StartChasingPlayer();
			}
		}
	}

	++scene_->multiLockChargeFrames_;
}

void LockOnManager::FireMultiLockMissiles() {
	if (!scene_->isMultiLockCharging_) {
		return;
	}

	PruneMultiLockTargets();
	if (scene_->multiLockTargets_.empty()) {
		if (scene_->aimAssistEnemy_) {
			MultiLockTarget altTarget{};
			altTarget.enemy = scene_->aimAssistEnemy_;
			altTarget.turretIndex = -1;
			altTarget.targetPoint = scene_->aimAssistEnemy_->GetAimPosition();
			altTarget.hasTargetPoint = false;
			scene_->multiLockTargets_.push_back(altTarget);
		} else if (scene_->lockedEnemy_ && IsLockedEnemyAlive()) {
			MultiLockTarget altTarget{};
			altTarget.enemy = scene_->lockedEnemy_;
			altTarget.turretIndex = scene_->lockedBossTurretIndex_;
			altTarget.targetPoint = GetLockedTargetPosition();
			altTarget.hasTargetPoint = (scene_->lockedBossTurretIndex_ >= 0);
			scene_->multiLockTargets_.push_back(altTarget);
		}
	}

	if (scene_->multiLockTargets_.empty()) {
		if (scene_->missilePresetManager_->FirePlayerMissile(MissileType::MissileWithTrail) &&
			scene_->tutorialEnabled_ && scene_->tutorialPhase_ == GamePlayScene::TutorialPhase::MultiLock) {
			++scene_->tutorialHomingLaunches_;
		}
	} else {
		const float spacing = 0.35f;
		const float center = (static_cast<float>(scene_->multiLockTargets_.size()) - 1.0f) * 0.5f;
		for (size_t index = 0; index < scene_->multiLockTargets_.size(); ++index) {
			const float horizontalOffset = (static_cast<float>(index) - center) * spacing;
			const auto &target = scene_->multiLockTargets_[index];
			const Vector3 *targetPointPtr = target.hasTargetPoint ? &target.targetPoint : nullptr;
			if (scene_->missilePresetManager_->FirePlayerMissile(MissileType::MissileWithTrail,
				target.enemy, horizontalOffset, true, targetPointPtr) &&
				scene_->tutorialEnabled_ && scene_->tutorialPhase_ == GamePlayScene::TutorialPhase::MultiLock) {
				++scene_->tutorialHomingLaunches_;
			}
		}
	}

	CancelMultiLock();
}

void LockOnManager::CancelMultiLock() {
	scene_->isMultiLockCharging_ = false;
	scene_->multiLockChargeFrames_ = 0;
	scene_->multiLockTargets_.clear();
}
