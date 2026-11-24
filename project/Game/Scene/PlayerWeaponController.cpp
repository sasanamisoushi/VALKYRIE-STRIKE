#include "PlayerWeaponController.h"

#include "AmmoManager.h"
#include "GameplayBalanceSettingsManager.h"
#include "LockOnManager.h"
#include "MissilePresetManager.h"
#include "MultiLockChargeState.h"
#include "SongFlightTrailRenderer.h"
#include "SongState.h"
#include "SpecialAttackState.h"
#include "TargetSelectionState.h"
#include "TutorialPhaseController.h"
#include "TutorialProgressTracker.h"
#include "Game/Player/Player.h"
#include "Game/enemy/Boss.h"
#include "Game/enemy/Enemy.h"
#include "engine/Audio/AudioManager.h"
#include "engine/Camera/Camera.h"
#include "engine/Input/Input.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/math/MyMath.h"

#include <cmath>

namespace {
constexpr char kSongBurstParticleGroup[] = "songSparkle";

Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

float LengthSqVector3(const Vector3& v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

Vector3 NormalizeOrVector3(const Vector3& v, const Vector3& fallback) {
	const float len = std::sqrt(LengthSqVector3(v));
	if (len <= 0.0001f) {
		return fallback;
	}
	return { v.x / len, v.y / len, v.z / len };
}
} // namespace

void PlayerWeaponController::Reset() {
	normalFireCooldownTimer_ = 0;
}

void PlayerWeaponController::AcquireNearestEnemyLockIfNeeded(const PlayerSpecialSongContext& context) const {
	if (!context.targetSelection || !context.player || !context.enemies) {
		return;
	}
	if (context.targetSelection->HasAliveLockedEnemy()) {
		return;
	}

	context.targetSelection->ClearLockedTarget();
	float minDistanceSq = 100000000.0f;
	Enemy* nearestEnemy = nullptr;
	const Vector3 playerPos = context.player->GetPosition();
	for (const auto& enemy : *context.enemies) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}
		const Vector3 difference = SubtractVector3(enemy->GetPosition(), playerPos);
		const float distanceSq = LengthSqVector3(difference);
		if (distanceSq < minDistanceSq) {
			minDistanceSq = distanceSq;
			nearestEnemy = enemy.get();
		}
	}
	context.targetSelection->SetLockedTarget(nearestEnemy, -1);
}

void PlayerWeaponController::UpdateSpecialAndSong(const PlayerSpecialSongContext& context) {
	if (!context.balanceSettings || !context.specialAttackState || !context.songState) {
		return;
	}

	const GameplayBalanceSettings& balance = *context.balanceSettings;
	SpecialAttackState& specialAttackState = *context.specialAttackState;
	SongState& songState = *context.songState;

	if (context.shouldUpdateGame && context.canUsePlayerInput && !specialAttackState.IsActive()) {
		if (context.ammoManager) {
			context.ammoManager->UpdateAutoReload();
		}
	}

	if (!context.isGameOverActive && context.shouldUpdateGame && context.canUsePlayerInput &&
		specialAttackState.CanStart(balance.specialAttackCost) &&
		Input::GetInstance()->TriggerAction(PlayerAction::SpecialAttack) &&
		specialAttackState.TryStart(balance.specialAttackCost)) {
		if (context.player) {
			context.player->ChangeMode(PlayerMode::Gerwalk);
			context.player->SetSpecialAttackActive(true);
		}
		AcquireNearestEnemyLockIfNeeded(context);
	}

	if (!context.isGameOverActive && context.shouldUpdateGame && context.canUsePlayerInput &&
		songState.CanActivate() && Input::GetInstance()->TriggerAction(PlayerAction::Song) &&
		songState.TryActivate()) {
		if (context.songFlightTrailRenderer) {
			context.songFlightTrailRenderer->Reset();
		}
		if (context.player) {
			context.player->SetSongActive(true);
		}
		// 発動直前にリロード中だった弾倉も即座に満タンへ戻す。以降は
		// TryConsumeAmmo() が消費を通さないので、歌の終了まで弾数は減らない。
		if (context.ammoManager) {
			context.ammoManager->ReloadAll();
		}
		if (context.player && context.particleManager) {
			const Vector3 forward = NormalizeOrVector3(context.player->GetForwardVector(), { 0.0f, 0.0f, 1.0f });
			const Vector3 burstOrigin = AddVector3(context.player->GetPosition(), { 0.0f, 0.20f, 0.0f });
			context.particleManager->EmitDirectionalBurst(
				kSongBurstParticleGroup, burstOrigin, { -forward.x, 0.34f - forward.y, -forward.z }, 40,
				{ 1.0f, 0.76f, 0.08f, 1.0f }, 0.86f,
				0.20f, 0.10f, 0.090f, 0.038f, 0.35f, 0.72f, 0.35f);
		}
		if (context.bgmVoice) context.bgmVoice->SetVolume(0.0f);
		if (context.songVoice) context.songVoice->SetVolume(1.0f);
	}

	if (songState.IsActive() && context.shouldUpdateGame) {
		if (songState.Advance(balance.songDurationFrames)) {
			if (context.songFlightTrailRenderer) {
				context.songFlightTrailRenderer->Reset();
			}
			if (context.player) {
				context.player->SetSongActive(false);
			}
			if (context.bgmVoice) context.bgmVoice->SetVolume(1.0f);
			if (context.songVoice) context.songVoice->SetVolume(0.0f);
		}
	}

	if (specialAttackState.IsActive()) {
		AcquireNearestEnemyLockIfNeeded(context);

		if (specialAttackState.IsFireFrame(balance.specialAttackFireIntervalFrames) && context.missilePresetManager) {
			context.missilePresetManager->FirePlayerMissile(MissileType::Normal, nullptr, -0.3f, false);
			context.missilePresetManager->FirePlayerMissile(MissileType::MissileWithTrail, nullptr, 0.3f, false);
		}
		if (specialAttackState.Advance(balance.specialAttackDurationFrames)) {
			if (context.player) {
				context.player->SetSpecialAttackActive(false);
			}
		}
	} else {
		specialAttackState.Recover(balance.spGaugeRecoveryPerSecond / 60.0f);
	}
}

void PlayerWeaponController::UpdateWeaponInput(const PlayerWeaponInputContext& context) {
	if (!context.specialAttackState || !context.multiLockChargeState || !context.lockOnManager) {
		return;
	}

	if (context.allowWeaponInput && context.player && !context.isGameOverActive && !context.specialAttackState->IsActive()) {
		Input* input = Input::GetInstance();

		// 通常射撃（長押しで連射可能）
		const bool isNormalFirePushed = input->PushMouseButton(0) || input->PushAction(PlayerAction::NormalFire);
		if (isNormalFirePushed) {
			if (normalFireCooldownTimer_ <= 0 && context.missilePresetManager) {
				Enemy* aimTarget = nullptr;
				Vector3 lockedTargetPosition{};
				const Vector3* lockedTargetPoint = nullptr;
				if (context.lockedEnemy && context.lockOnManager->IsLockedEnemyAlive()) {
					aimTarget = context.lockedEnemy;
					if (context.lockedEnemy->IsBoss()) {
						lockedTargetPosition = context.lockOnManager->GetLockedTargetPosition();
						lockedTargetPoint = &lockedTargetPosition;
					}
				} else if (context.aimAssistEnemy) {
					aimTarget = context.aimAssistEnemy;
				}
				if (context.missilePresetManager->FirePlayerMissile(MissileType::Normal, aimTarget, 0.0f, true, lockedTargetPoint)) {
					if (context.tutorialController && context.tutorialProgress &&
						context.tutorialController->IsEnabled() &&
						context.tutorialController->GetPhase() == TutorialPhase::NormalFire) {
						context.tutorialProgress->RecordNormalShot();
					}
					normalFireCooldownTimer_ = kNormalFireCooldownFrames; // 8フレームごとに連続発射
				}
			}
		} else {
			normalFireCooldownTimer_ = 0;
		}

		if (normalFireCooldownTimer_ > 0) {
			--normalFireCooldownTimer_;
		}

		// 右クリックはホーミングミサイル射撃 (全モード共通)
		if (input->TriggerMouseButton(1) || input->TriggerAction(PlayerAction::HomingFire)) {
			context.lockOnManager->BeginMultiLock();
		}
		if (context.multiLockChargeState->IsCharging() && (input->PushMouseButton(1) || input->PushAction(PlayerAction::HomingFire))) {
			context.lockOnManager->UpdateMultiLock(context.activeCamera);
		}
		if (context.multiLockChargeState->IsCharging() && !input->PushMouseButton(1) && !input->PushAction(PlayerAction::HomingFire)) {
			context.lockOnManager->FireMultiLockMissiles();
		}
	} else if (context.multiLockChargeState->IsCharging()) {
		context.lockOnManager->CancelMultiLock();
	}
}
