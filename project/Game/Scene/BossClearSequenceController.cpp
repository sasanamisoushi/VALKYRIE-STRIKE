#include "BossClearSequenceController.h"

#include "Game/Player/Player.h"
#include "Game/enemy/Boss.h"
#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/math/MyMath.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>

namespace {
float SmoothStep01(float value) {
	value = std::clamp(value, 0.0f, 1.0f);
	return value * value * (3.0f - 2.0f * value);
}

Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

void EmitDebrisBlackSmoke(Boss* boss, ParticleManager* particles, bool groundedWreckageOnly = false) {
	if (!boss || !particles) {
		return;
	}
	const std::vector<Vector3> debrisPositions = groundedWreckageOnly
		? boss->GetCinematicWreckageWorldPositions()
		: boss->GetCinematicDebrisWorldPositions();
	for (size_t i = 0; i < debrisPositions.size(); ++i) {
		particles->EmitSmoke(
			"blackSmoke",
			debrisPositions[i],
			1,
			{ 0.06f, 0.06f, 0.07f, 0.48f },
			1.2f,
			3.4f,
			0.035f,
			0.015f,
			0.65f,
			1.15f,
			0.35f);
	}
}

Vector3 RandomPointInBossBody(const OBB& bossObb, std::mt19937& randomEngine) {
	std::uniform_real_distribution<float> distBody(-0.78f, 0.78f);
	const float rx = distBody(randomEngine) * bossObb.size.x;
	const float ry = distBody(randomEngine) * bossObb.size.y;
	const float rz = distBody(randomEngine) * bossObb.size.z;
	return {
		bossObb.center.x + bossObb.orientations[0].x * rx + bossObb.orientations[1].x * ry + bossObb.orientations[2].x * rz,
		bossObb.center.y + bossObb.orientations[0].y * rx + bossObb.orientations[1].y * ry + bossObb.orientations[2].y * rz,
		bossObb.center.z + bossObb.orientations[0].z * rx + bossObb.orientations[1].z * ry + bossObb.orientations[2].z * rz,
	};
}
}

void BossClearSequenceController::Initialize() {
	smokeTrailManager_.Initialize();
}

void BossClearSequenceController::Reset() {
	phaseState_.Reset();
	timeline_.Reset();
	effectState_.Reset();
	crashState_.Reset();
	focusState_.Reset();
	cameraState_.Reset();
	playerExitState_.Reset();
	smokeTrailManager_.Reset();
}

bool BossClearSequenceController::TryBegin(
	Enemy* defeatedEnemy,
	Player* player,
	const std::function<void(Enemy*)>& clearEnemyReferences) {
	if (!defeatedEnemy || !phaseState_.Is(BossClearPhase::None)) {
		return false;
	}

	const Vector3 defeatedPosition = defeatedEnemy->GetPosition();
	// 撃破済みでも消さずに残して、ここから専用の終了演出を開始する。
	if (clearEnemyReferences) {
		clearEnemyReferences(defeatedEnemy);
	}
	Reset();
	if (Boss* defeatedBossModel = dynamic_cast<Boss*>(defeatedEnemy)) {
		// OBJ の原点ではなく、描画モデルの境界中心を演出の基準にする。
		crashState_.Start(defeatedBossModel->GetRotation(), defeatedBossModel->GetVelocity());
		focusState_.Set(
			defeatedBossModel->GetBodyLockOnPosition(), defeatedBossModel->GetCollisionRadius());
		const float horizSpeed = std::hypot(crashState_.GetVelocity().x, crashState_.GetVelocity().z);
		if (horizSpeed < 2.0f) {
			crashState_.Velocity().x = std::sin(crashState_.GetInitialRotation().y) * 14.0f;
			crashState_.Velocity().z = std::cos(crashState_.GetInitialRotation().y) * 14.0f;
		}
		if (crashState_.GetVelocity().y > -2.5f) {
			crashState_.Velocity().y = -2.5f;
		}
		smokeTrailManager_.SetOrigin(0, focusState_.GetPosition());
		const size_t thrusterCount = defeatedBossModel->GetRearThrusterCount();
		if (thrusterCount > 0) {
			smokeTrailManager_.SetOrigin(1, defeatedBossModel->GetRearThrusterWorldPosition(0));
			smokeTrailManager_.SetOrigin(2, defeatedBossModel->GetRearThrusterWorldPosition(thrusterCount - 1));
		}
	} else {
		crashState_.Start({}, {});
		focusState_.Set(defeatedPosition, 1.0f);
	}

	phaseState_.Set(BossClearPhase::DefeatCinematic);
	timeline_.ResetCinematicFrames();
	if (player) {
		player->SetPresentationAlpha(0.0f);
		player->SetPresentationThrustersVisible(false);
	}
	return true;
}

bool BossClearSequenceController::Update(
	bool isPaused,
	std::list<std::unique_ptr<Enemy>>& enemies,
	const std::list<std::unique_ptr<Obstacle>>& obstacles,
	Player* player,
	ExplosionManager* explosionManager,
	ParticleManager* particles,
	const std::function<void(Enemy*)>& clearEnemyReferences) {
	if (!phaseState_.IsActive()) {
		return false;
	}

	if (isPaused) {
		timeline_.Pause();
		return false;
	}

	const float dt = timeline_.Advance(std::chrono::steady_clock::now());

	Boss* cinematicBoss = nullptr;
	for (const auto& enemy : enemies) {
		if (enemy && enemy->IsBoss() && enemy->IsDead()) {
			cinematicBoss = static_cast<Boss*>(enemy.get());
			break;
		}
	}

	if (phaseState_.Is(BossClearPhase::DefeatCinematic) && cinematicBoss) {
		// フェーズ1：誘爆と墜落（Timer: 0.0s ～ 3.0s）
		const float progress = std::clamp(timeline_.GetPhaseSeconds() / kPhase1DurationSec, 0.0f, 1.0f);

		// 慣性の法則と重力落下（回転は加えず、撃破時の姿勢のままくっついて地面へ落下する）
		constexpr float kCrashGravity = 16.0f;
		crashState_.Velocity().y -= kCrashGravity * dt;
		const float airDrag = std::pow(0.992f, dt * 60.0f);
		crashState_.Velocity().x *= airDrag;
		crashState_.Velocity().z *= airDrag;

		Vector3 bossPos = cinematicBoss->GetPosition();
		bossPos.x += crashState_.GetVelocity().x * dt;
		bossPos.y += crashState_.GetVelocity().y * dt;
		bossPos.z += crashState_.GetVelocity().z * dt;

		cinematicBoss->SetPosition(bossPos);
		cinematicBoss->SetRotation(crashState_.GetInitialRotation());
		cinematicBoss->SetCinematicWreckPresentation(0.35f + progress * 0.45f);
		cinematicBoss->UpdateModel();
		focusState_.SetPosition(cinematicBoss->GetBodyLockOnPosition());

		// 煙の発生：ボスのモデル全体（OBB内のあちこち）から小さく生まれて大きく膨らみ上へ立ち上る黒煙
		static std::mt19937 crashRng(std::random_device{}());
		if (effectState_.ShouldEmitSmoke(dt, 0.035f) && particles) {
			const OBB bossObb = cinematicBoss->GetOBB();
			std::uniform_real_distribution<float> distBody(-0.88f, 0.88f);
			const float initScale = (std::max)(1.8f, focusState_.GetRadius() * 0.22f);
			const float finalScale = (std::max)(5.5f, focusState_.GetRadius() * 0.72f);

			for (int s = 0; s < 5; ++s) {
				const float rx = distBody(crashRng) * bossObb.size.x;
				const float ry = distBody(crashRng) * bossObb.size.y;
				const float rz = distBody(crashRng) * bossObb.size.z;
				const Vector3 smokePos = {
					bossObb.center.x + bossObb.orientations[0].x * rx + bossObb.orientations[1].x * ry + bossObb.orientations[2].x * rz,
					bossObb.center.y + bossObb.orientations[0].y * rx + bossObb.orientations[1].y * ry + bossObb.orientations[2].y * rz,
					bossObb.center.z + bossObb.orientations[0].z * rx + bossObb.orientations[1].z * ry + bossObb.orientations[2].z * rz,
				};
				particles->EmitSmoke(
					"blackSmoke",
					smokePos,
					1,
					{ 0.06f, 0.06f, 0.07f, 0.46f },
					initScale,
					finalScale,
					0.055f,
					0.020f,
					0.9f,
					1.5f,
					0.6f);
			}
		}

		// 煙だけで落下させず、船体の内部から小さな誘爆を連続発生させる。
		if (effectState_.ShouldEmitFallingExplosion(dt, 0.18f) && particles) {
			const OBB bossObb = cinematicBoss->GetOBB();
			const Vector3 burstPosition = RandomPointInBossBody(bossObb, crashRng);
			const float burstScale = (std::max)(0.65f, focusState_.GetRadius() * 0.075f);
			particles->EmitFireball(
				"explosionCore", burstPosition, 5,
				{ 1.0f, 0.34f, 0.035f, 0.94f },
				burstScale * 0.45f, burstScale * 1.45f, 0.32f, burstScale * 0.20f);
			particles->EmitDirectionalBurst(
				"test", burstPosition, { 0.0f, 1.0f, 0.0f }, 32,
				{ 1.0f, 0.42f, 0.08f, 0.90f }, 3.14159265f,
				burstScale * 3.0f, burstScale, burstScale * 0.12f, burstScale * 0.06f,
				0.24f, 0.42f, burstScale * 0.28f);
			particles->EmitSmoke(
				"blackSmoke", burstPosition, 3,
				{ 0.06f, 0.06f, 0.07f, 0.48f },
				burstScale * 0.65f, burstScale * 1.75f, 0.065f, 0.035f, 0.65f, 1.05f, burstScale * 0.45f);
		}

		// 地形への激突判定（地面にぶつかった瞬間に大爆破＆破片飛散へ移行）
		bool hitTerrain = false;
		const Sphere bossCrashSphere{ focusState_.GetPosition(), (std::max)(2.0f, focusState_.GetRadius() * 0.55f) };
		float wreckageRestY = bossCrashSphere.center.y - bossCrashSphere.radius;
		for (const auto& obstacle : obstacles) {
			if (!obstacle || !obstacle->IsCollisionEnabled()) {
				continue;
			}
			if (obstacle->IsStageBounds()) {
				const float floorY = obstacle->GetPosition().y - obstacle->GetWorldHalfExtents().y;
				if (focusState_.GetPosition().y - bossCrashSphere.radius <= floorY) {
					hitTerrain = true;
					wreckageRestY = floorY;
					break;
				}
			} else if (obstacle->IsUseMeshCollider()) {
				const auto& nearbyIndices = obstacle->GetNearbyWorldTriangleIndices(bossCrashSphere);
				const auto& tris = obstacle->GetWorldTriangles();
				Vector3 pushOut{};
				for (size_t triIdx : nearbyIndices) {
					if (triIdx < tris.size() && MyMath::IsCollision(bossCrashSphere, tris[triIdx], pushOut)) {
						hitTerrain = true;
						wreckageRestY = (std::max)({ tris[triIdx].p[0].y, tris[triIdx].p[1].y, tris[triIdx].p[2].y });
						break;
					}
				}
				if (hitTerrain) {
					break;
				}
			} else if (MyMath::IsCollision(bossCrashSphere, obstacle->GetOBB())) {
				hitTerrain = true;
				wreckageRestY = obstacle->GetOBB().center.y + obstacle->GetOBB().size.y;
				break;
			}
		}

		if (hitTerrain || timeline_.GetPhaseSeconds() >= 10.0f) {
			cinematicBoss->SetCinematicDebrisRestPlane(wreckageRestY);
			phaseState_.Set(BossClearPhase::FinalExplosion);
			timeline_.ResetPhase();
			effectState_.ResetSmokeTimer();
			timeline_.ResetCinematicFrames();
		}
	} else if (phaseState_.Is(BossClearPhase::FinalExplosion) && cinematicBoss) {
		// フェーズ2：大爆発と爆散（Timer: 3.0s ～ 3.5s）
		if (timeline_.GetCinematicFrames() == 1) {
			const Vector3 explosionCenter = cinematicBoss->GetBodyLockOnPosition();
			cameraState_.SetExplosionCenter(explosionCenter);
			focusState_.SetPosition(explosionCenter);
			cinematicBoss->SetCinematicWreckPresentation(1.0f);
			cinematicBoss->BeginCinematicDebris();

			const float radius = (std::max)(4.0f, focusState_.GetRadius() * 0.72f);
			if (explosionManager) {
				explosionManager->CreateDestructionEffects({
					explosionCenter,
					{ explosionCenter.x + radius, explosionCenter.y, explosionCenter.z },
					{ explosionCenter.x - radius, explosionCenter.y, explosionCenter.z },
					{ explosionCenter.x, explosionCenter.y + radius * 0.5f, explosionCenter.z },
					{ explosionCenter.x, explosionCenter.y, explosionCenter.z + radius },
				}, ExplosionManager::EffectTarget::Boss);
			}
			if (particles) {
				particles->EmitFireball("explosionCore", explosionCenter, 112,
					{ 1.0f, 0.58f, 0.08f, 1.0f }, radius * 0.35f, radius * 2.1f, 1.35f, radius * 0.68f);
				particles->EmitDirectionalBurst("test", explosionCenter, { 0.0f, 1.0f, 0.0f }, 600,
					{ 1.0f, 0.36f, 0.04f, 1.0f }, 3.14159265f,
					radius * 0.14f, radius * 0.06f, radius * 0.055f, radius * 0.025f,
					0.85f, 1.45f, radius * 0.35f);
				particles->EmitSmoke(
					"blackSmoke",
					explosionCenter,
					48,
					{ 0.06f, 0.06f, 0.07f, 0.52f },
					radius * 0.45f,
					radius * 1.45f,
					0.065f,
					0.045f,
					1.2f,
					2.1f,
					radius * 0.55f);
			}
		}

		cinematicBoss->StepCinematicDebris(dt);
		if (effectState_.ShouldEmitSmoke(dt, 0.035f)) {
			EmitDebrisBlackSmoke(cinematicBoss, particles);
		}

		if (timeline_.GetPhaseSeconds() >= kPhase2FlashDurationSec) {
			// 自機の離脱カットは使わず、破片が広がる余韻を見せてから MISSION CLEAR へ移る。
			phaseState_.Set(BossClearPhase::DebrisAftermath);
			timeline_.ResetPhase();
			effectState_.ResetSmokeTimer();
			timeline_.ResetCinematicFrames();
		}
	} else if (phaseState_.Is(BossClearPhase::Wreck) ||
		phaseState_.Is(BossClearPhase::DebrisAftermath) ||
		phaseState_.Is(BossClearPhase::MissionClear)) {
		// 爆散後のデブリ・黒煙を維持する。プレイヤー離脱は Wreck フェーズだけに限定する。
		if (cinematicBoss) {
			cinematicBoss->StepCinematicDebris(dt);
			if (effectState_.ShouldEmitSmoke(dt, 0.04f)) {
				EmitDebrisBlackSmoke(cinematicBoss, particles, true);
			}
		}

		if (phaseState_.Is(BossClearPhase::Wreck) && effectState_.TryInitializePhase3()) {
			const Vector3 camForward = cameraState_.ComputeForwardDirection();
			const Vector3 camRight = cameraState_.RefreshSlideDirectionFromAnchor();
			const Vector3& anchorPos = cameraState_.GetAnchorPosition();

			playerExitState_.Position() = {
				anchorPos.x + camForward.x * 15.0f - camRight.x * 7.5f,
				anchorPos.y + camForward.y * 15.0f - 2.0f,
				anchorPos.z + camForward.z * 15.0f - camRight.z * 7.5f,
			};
			playerExitState_.Direction() = MyMath::Normalize(Vector3{
				camRight.x * 0.84f - camForward.x * 0.24f,
				0.38f,
				camRight.z * 0.84f - camForward.z * 0.24f,
			});
			playerExitState_.Speed() = 0.58f;
			playerExitState_.Roll() = -0.16f;

			if (player) {
				player->ChangeMode(PlayerMode::Fighter);
				player->SetPresentationAlpha(1.0f);
				player->SetPresentationThrustersVisible(true);
				player->SetBoosterPlumeEnabled(true);
			}
		}

		if (phaseState_.Is(BossClearPhase::Wreck) && player) {
			const float flyProgress = std::clamp(
				timeline_.GetPhaseSeconds() / kPhase3PlayerEscapeSec, 0.0f, 1.0f);
			constexpr float kExitStartProgress = 0.87f;
			const float inFrameProgress = SmoothStep01((std::min)(flyProgress / kExitStartProgress, 1.0f));
			const float exitProgress = SmoothStep01(
				(std::max)(0.0f, (flyProgress - kExitStartProgress) / (1.0f - kExitStartProgress)));

			const Vector3 cameraForward = cameraState_.ComputeForwardDirection();
			const Vector3 cameraRight = cameraState_.ComputeSlideDirection();
			const Vector3 cameraPosition = cameraState_.EvaluateAftermathCameraPosition(timeline_.GetPhaseSeconds());

			const float screenSide = -7.0f + 12.0f * inFrameProgress + 10.5f * exitProgress;
			const float screenHeight = 2.4f - 3.2f * inFrameProgress - 3.5f * exitProgress;
			const float depth = 28.0f - 22.0f * inFrameProgress - 2.0f * exitProgress;
			Vector3 playerPosition = {
				cameraPosition.x + cameraForward.x * depth + cameraRight.x * screenSide,
				cameraPosition.y + cameraForward.y * depth + cameraRight.y * screenSide + screenHeight,
				cameraPosition.z + cameraForward.z * depth + cameraRight.z * screenSide,
			};

			float avoidSide = 0.0f;
			float avoidHeight = 0.0f;
			if (cinematicBoss) {
				const std::vector<Vector3> debrisPositions = cinematicBoss->GetCinematicDebrisWorldPositions();
				for (size_t debrisIndex = 0; debrisIndex < debrisPositions.size(); ++debrisIndex) {
					const Vector3 toPlayer = SubtractVector3(playerPosition, debrisPositions[debrisIndex]);
					const float depthOffset = toPlayer.x * cameraForward.x + toPlayer.y * cameraForward.y + toPlayer.z * cameraForward.z;
					const float sideOffset = toPlayer.x * cameraRight.x + toPlayer.y * cameraRight.y + toPlayer.z * cameraRight.z;
					const float heightOffset = toPlayer.y;
					if (std::abs(depthOffset) > 5.5f || std::abs(sideOffset) > 3.3f || std::abs(heightOffset) > 2.6f) {
						continue;
					}
					const float proximity = 1.0f - (std::abs(depthOffset) / 5.5f +
						std::abs(sideOffset) / 3.3f + std::abs(heightOffset) / 2.6f) / 3.0f;
					const float safeSide = std::abs(sideOffset) > 0.12f
						? (sideOffset > 0.0f ? 1.0f : -1.0f)
						: (debrisIndex % 2 == 0 ? 1.0f : -1.0f);
					const float safeHeight = std::abs(heightOffset) > 0.12f
						? (heightOffset > 0.0f ? 1.0f : -1.0f)
						: (debrisIndex % 3 == 0 ? 1.0f : -1.0f);
					avoidSide += safeSide * proximity * 1.35f;
					avoidHeight += safeHeight * proximity * 0.85f;
				}
			}
			avoidSide = std::clamp(avoidSide, -2.2f, 2.2f) * (1.0f - exitProgress);
			avoidHeight = std::clamp(avoidHeight, -1.45f, 1.45f) * (1.0f - exitProgress);
			playerPosition.x += cameraRight.x * avoidSide;
			playerPosition.y += cameraRight.y * avoidSide + avoidHeight;
			playerPosition.z += cameraRight.z * avoidSide;
			playerExitState_.Position() = playerPosition;
			playerExitState_.Direction() = MyMath::Normalize(Vector3{
				cameraRight.x * (0.90f + exitProgress * 0.08f) - cameraForward.x * 0.30f,
				-0.22f + avoidHeight * 0.18f - exitProgress * 0.34f,
				cameraRight.z * (0.90f + exitProgress * 0.08f) - cameraForward.z * 0.30f,
			});
			playerExitState_.Speed() = 0.82f + exitProgress * 0.30f;
			playerExitState_.Roll() = 0.08f + exitProgress * 0.20f;

			const float horizLen = std::hypot(playerExitState_.GetDirection().x, playerExitState_.GetDirection().z);
			const float yaw = std::atan2(playerExitState_.GetDirection().x, playerExitState_.GetDirection().z);
			const float pitch = -std::atan2(playerExitState_.GetDirection().y, (std::max)(0.0001f, horizLen));
			player->UpdatePresentation(
				playerExitState_.GetPosition(), { pitch, yaw, playerExitState_.GetRoll() }, playerExitState_.GetSpeed(), true);
		}

		if (phaseState_.Is(BossClearPhase::DebrisAftermath) &&
			timeline_.GetPhaseSeconds() >= kDebrisAftermathDurationSec) {
			phaseState_.Set(BossClearPhase::MissionClear);
			timeline_.ResetPhase();
			timeline_.ResetCinematicFrames();
			effectState_.StartMissionClearUi();
		} else if (phaseState_.Is(BossClearPhase::Wreck) &&
			timeline_.GetPhaseSeconds() >= kPhase3PlayerEscapeSec) {
			// 機体が画面外へ向かうタイミングで「MISSION CLEAR」をフェードイン開始
			phaseState_.Set(BossClearPhase::MissionClear);
			timeline_.ResetPhase();
			timeline_.ResetCinematicFrames();
			effectState_.StartMissionClearUi();
		} else if (phaseState_.Is(BossClearPhase::MissionClear)) {
			effectState_.AdvanceMissionClearUi(dt);
			if (effectState_.GetMissionClearUiSeconds() >= kMissionClearDurationSec) {
				for (auto it = enemies.begin(); it != enemies.end(); ++it) {
					if (*it && (*it)->IsBoss() && (*it)->IsDead()) {
						if (clearEnemyReferences) {
							clearEnemyReferences(it->get());
						}
						enemies.erase(it);
						break;
					}
				}
				return true;
			}
		}
	}

	return false;
}
