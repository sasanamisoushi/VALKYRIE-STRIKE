#include "GameplayUpdateCoordinator.h"

#include "3D/Model.h"
#include "BossClearSequenceController.h"
#include "CinematicCameraCoordinator.h"
#include "DamageFeedbackState.h"
#include "EnemyDefeatEffectController.h"
#include "EnemyUpdateCoordinator.h"
#include "GamePlayUIManager.h"
#include "TitleBackgroundEnemyController.h"
#include "TitleBackgroundEnemyFlightController.h"
#include "TitleBackgroundEnemyState.h"
#include "TitleBackgroundEnemyWeaponController.h"
#include "Game/Player/Player.h"
#include "Game/Scene/ResultData.h"
#include "engine/Scene/SceneManager.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/GroundEnemy.h"
#include "Game/obstacle/Obstacle.h"
#include "GameCameraManager.h"
#include "GameOverSequence.h"
#include "GameplayAudioController.h"
#include "LockOnManager.h"
#include "PlayerFlightEffectController.h"
#include "SceneDebugEditorController.h"
#include "SimulationManager.h"
#include "TargetSelectionState.h"
#include "TitleLaunchState.h"
#include "TutorialPhaseController.h"
#include "WarpExitPresentationState.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/Particle/ParticleManager.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
	Vector3 SubtractVec3(const Vector3& a, const Vector3& b) {
		return { a.x - b.x, a.y - b.y, a.z - b.z };
	}

	Vector3 CalculateTitleLaunchCameraShake(float launchProgress, int launchFrame) {
		const float ignitionShake = std::clamp(1.0f - std::abs(launchProgress - 0.14f) / 0.14f, 0.0f, 1.0f);
		const float warpShockShake = std::clamp(1.0f - std::abs(launchProgress - 0.77f) / 0.13f, 0.0f, 1.0f);
		const float sustainedBoost = std::clamp((launchProgress - 0.18f) / 0.56f, 0.0f, 1.0f) * (1.0f - launchProgress * 0.25f);
		const float amplitude = ignitionShake * ignitionShake * 0.085f +
			sustainedBoost * 0.042f +
			warpShockShake * warpShockShake * 0.14f;
		if (amplitude <= 0.0001f) {
			return { 0.0f, 0.0f, 0.0f };
		}
		const float phase = static_cast<float>(launchFrame);
		return {
			(std::sin(phase * 1.97f) + std::cos(phase * 3.11f) * 0.5f) * amplitude,
			(std::cos(phase * 2.43f) + std::sin(phase * 3.79f) * 0.5f) * amplitude * 0.8f,
			std::sin(phase * 2.89f) * amplitude * 0.35f,
		};
	}
}

bool GameplayUpdateCoordinator::UpdateEnemyAndWavePhase(const GameplayEnemyPhaseContext& context) const {
	if (!context.enemies) {
		return false;
	}
	if (!context.updateSelectedEnemies) {
		for (auto& enemy : *context.enemies) {
			enemy->UpdateModel();
		}
		return false;
	}

	Vector3 playerPos = context.player ? context.player->GetOBB().center : Vector3{ 0.0f, 0.0f, 0.0f };
	if (context.enemyUpdateCoordinator) {
		ParticleManager* particleManager = context.particleManager;
		EnemyDefeatEffectController* defeatEffects = context.enemyDefeatEffectController;
		context.enemyUpdateCoordinator->UpdateEnemies({
			context.isAutoFlightMode,
			context.bossMaxActiveMinions,
			context.player,
			context.enemies,
			context.obstacles,
			context.enemyBulletManager,
			context.explosionManager,
			particleManager,
			context.combatAlertState,
			[defeatEffects, particleManager](GroundEnemy& enemy) {
				if (defeatEffects) defeatEffects->SpawnNormalEnemyDebris(enemy, particleManager);
			},
			[defeatEffects](GroundEnemy& enemy) {
				if (defeatEffects) defeatEffects->SpawnHomingEnemyImplosion(enemy);
			},
		}, playerPos);
		context.enemyUpdateCoordinator->ProcessDefeatedEnemies({
			context.isAutoFlightMode,
			context.isGameOverActive,
			context.player,
			context.enemies,
			context.enemySpawns,
			context.enemyEventManager,
			context.enemySpawnManager,
			context.songState,
			context.bossClearSequence,
			context.titleBackgroundEnemyState,
			context.defeatedEnemyCount,
			context.clearEnemyReferences,
		});
	}

	if (context.isAutoFlightMode && context.player && context.targetSelection && context.titleBackgroundEnemyState) {
		if (!context.targetSelection->HasAliveLockedEnemy() && context.titleBackgroundEnemyController) {
			context.targetSelection->SetLockedTarget(
				context.titleBackgroundEnemyController->AcquireTarget(
					*context.player, *context.enemies, *context.titleBackgroundEnemyState), -1);
		}
		if (Enemy* lockedEnemy = context.targetSelection->GetLockedEnemy()) {
			if (context.titleBackgroundEnemyFlightController && context.camera && context.obstacles) {
				context.titleBackgroundEnemyFlightController->Update(
					*context.player, *lockedEnemy, *context.camera,
					*context.obstacles, *context.titleBackgroundEnemyState, context.titleBackgroundFrame);
			}
			if (context.titleBackgroundEnemyWeaponController) {
				context.titleBackgroundEnemyWeaponController->Update(
					*lockedEnemy, *context.titleBackgroundEnemyState, context.missilePresetManager);
			}
		}
	}

	if (context.enemyUpdateCoordinator) {
		Vector3 targetBossPos = playerPos;
		const Boss::Tuning* bossTuning = nullptr;
		if (context.uiManager) {
			targetBossPos = context.uiManager->GetBossBattlePosition();
			bossTuning = &context.uiManager->GetBossTuning();
		}
		if (context.enemyUpdateCoordinator->UpdateWaveAndBossProgression({
				context.isAutoFlightMode,
				context.isGameOverActive,
				targetBossPos,
				bossTuning,
				context.enemySpawns,
				context.enemies,
				context.obstacles,
				context.enemySpawnManager,
				context.bossEncounterState,
			})) {
			return true;
		}
	}

	if (context.obstacles) {
		for (auto& obstacle : *context.obstacles) {
			obstacle->Update();
		}
	}
	return false;
}

bool GameplayUpdateCoordinator::UpdateGameOverPhase(
	const GameplayGameOverUpdateContext& context, bool& shouldUpdateGame) const {
	if (!context.gameOverSequence) {
		return false;
	}

	if (!context.isTitleBackgroundMode && !context.gameOverSequence->IsActive() &&
		(!context.isAttractDemoMode || !context.isAttractClearCinematic) &&
		!context.isBossClearSequenceActive && context.player && context.player->IsDead()) {
		context.gameOverSequence->Start(context.player->GetPosition());
		// 撃墜したフレームに ESC が重なっても、ゲームオーバー演出を停止させない。
		if (context.cancelPauseForCinematic) {
			context.cancelPauseForCinematic();
		}
		SceneManager::GetInstance()->SetMissionResult({
			MissionResultOutcome::Failed,
			context.defeatedEnemyCount,
			context.player->GetHP(),
			context.gameplayElapsedFrames ? *context.gameplayElapsedFrames : 0,
			context.isAttractDemoMode,
			(context.tutorialController && context.tutorialController->IsEnabled())
				? MissionStage::Tutorial : MissionStage::Standard,
		});
		context.player->SetPresentationThrustersVisible(false);

		std::vector<Vector3> playerHitPos = { context.player->GetPosition() };
		if (context.explosionManager) {
			context.explosionManager->CreateDestructionEffects(
				playerHitPos, ExplosionManager::EffectTarget::Player);
		}
		if (context.audioController) {
			context.audioController->StopBgmForGameOver();
		}
	}

	if (context.gameOverSequence->IsActive()) {
		context.gameOverSequence->Advance();
		// 装甲の各所が時間差で破裂するよう、機体中心の周りに爆発を散らす。
		// 更新頻度も1/5にすることで、世界全体がスローになる演出を作る。
		const int frame = context.gameOverSequence->GetFrame();
		if (context.explosionManager && frame <= 108 && frame % 18 == 0) {
			const float burstIndex = static_cast<float>(frame / 18);
			const Vector3 focus = context.gameOverSequence->GetFocusPosition();
			const Vector3 burstPosition = {
				focus.x + std::cos(burstIndex * 2.41f) * 1.75f,
				focus.y + std::sin(burstIndex * 1.73f) * 1.10f,
				focus.z + std::sin(burstIndex * 2.41f) * 1.75f,
			};
			context.explosionManager->CreateDestructionEffects(
				{ burstPosition }, ExplosionManager::EffectTarget::Player);
			if (context.particleManager) {
				context.particleManager->EmitDirectionalBurst("test", burstPosition,
					SubtractVec3(burstPosition, focus), 180,
					{ 1.0f, 0.34f, 0.05f, 0.95f }, 1.15f,
					1.35f, 0.75f, 0.055f, 0.035f, 0.70f, 1.45f, 0.95f);
			}
		}

		if (PostEffect::GetInstance()) {
			float effectProgress = static_cast<float>(frame) / 120.0f;
			if (effectProgress > 1.0f) {
				effectProgress = 1.0f;
			}
			const float vignetteRadius = 0.62f - 0.22f * effectProgress;
			const float blurIntensity = 1.5f + 3.0f * effectProgress;
			PostEffect::GetInstance()->SetVignetteSmoothing(vignetteRadius, 0.38f, blurIntensity);
		}

		shouldUpdateGame = (frame % 5 == 0);
		return context.gameOverSequence->HasFinished();
	}

	if (!context.isTitleBackgroundMode && shouldUpdateGame && context.gameplayElapsedFrames) {
		++(*context.gameplayElapsedFrames);
	}
	if (context.playerFlightEffectController && context.damageFeedbackState) {
		context.playerFlightEffectController->UpdatePostEffect(context.player, *context.damageFeedbackState);
	}
	return false;
}

void GameplayUpdateCoordinator::UpdateDebugAnimationAndBones(const GameplayDebugAnimationContext& context) const {
	if (context.myBox && context.animationData && context.skeleton && context.animationTime &&
		context.animationData->duration > 0.0f) {
		if (context.playAnimation && context.updateAnimationPreview) {
			*context.animationTime += 1.0f / 60.0f;
			*context.animationTime = std::fmod(*context.animationTime, context.animationData->duration);
		}

		ApplyAnimation(*context.skeleton, *context.animationData, *context.animationTime);
		::Update(*context.skeleton);
		if (context.enableSkinning && context.myModelObject && context.myModelObject->GetModel()) {
			context.myModelObject->UpdateSkinCluster(*context.skeleton);
		}

		if (!context.skeleton->joints.empty()) {
			const auto& rootTransform = context.skeleton->joints[context.skeleton->root].transform;
			context.myBox->SetTranslate(rootTransform.translate);
			context.myBox->SetQuaternionRotate(rootTransform.rotate);
			context.myBox->SetScale(rootTransform.scale);

			if (context.myModelObject && context.myModelObject->GetModel()) {
				if (!context.myModelObject->HasValidSkinCluster()) {
					context.myModelObject->SetTranslate(rootTransform.translate);
					context.myModelObject->SetQuaternionRotate(rootTransform.rotate);
					context.myModelObject->SetScale(rootTransform.scale);
				} else {
					context.myModelObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
					context.myModelObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
					context.myModelObject->SetScale({ context.modelScale, context.modelScale, context.modelScale });
				}
			}
		}

		if (context.showBones && context.player && context.skeletonLinesObject) {
			std::vector<VertexData> lineVertices;
			const Skeleton& playerSkeleton = context.player->GetSkeleton();
			for (size_t i = 0; i < playerSkeleton.joints.size(); ++i) {
				const Vector3 pos = {
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][0],
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][1],
					playerSkeleton.joints[i].skeletonSpaceMatrix.m[3][2]
				};

				if (playerSkeleton.joints[i].parent) {
					const int32_t parentIndex = *playerSkeleton.joints[i].parent;
					const Vector3 parentPos = {
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][0],
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][1],
						playerSkeleton.joints[parentIndex].skeletonSpaceMatrix.m[3][2]
					};

					VertexData v1{}, v2{};
					v1.position = { parentPos.x, parentPos.y, parentPos.z, 1.0f };
					v1.normal = { 0.0f, 1.0f, 0.0f };
					v1.texcoord = { 0.0f, 0.0f };

					v2.position = { pos.x, pos.y, pos.z, 1.0f };
					v2.normal = { 0.0f, 1.0f, 0.0f };
					v2.texcoord = { 1.0f, 1.0f };

					const Vector4 color = (context.simulationManager &&
						context.simulationManager->IsBoneSelected(playerSkeleton.joints[i].name))
						? Vector4{ 1.0f, 1.0f, 0.0f, 1.0f }
						: Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
					v1.color = color;
					v2.color = color;

					lineVertices.push_back(v1);
					lineVertices.push_back(v2);
				}
			}

			if (context.player->GetObject3d()) {
				context.skeletonLinesObject->SetTranslate(context.player->GetPosition());
				context.skeletonLinesObject->SetQuaternionRotate(context.player->GetQuaternion());
				context.skeletonLinesObject->SetScale(context.player->GetObject3d()->GetScale());
			}
			if (!lineVertices.empty() && context.skeletonLinesObject->GetModel()) {
				context.skeletonLinesObject->GetModel()->UpdateLineVertices(lineVertices);
			}
			context.skeletonLinesObject->Update();
		}
	}

	if (context.showModel && context.myModelObject) {
		context.myModelObject->Update();
	}
}

void GameplayUpdateCoordinator::UpdateSceneCamera(const GameplayCameraUpdateContext& context) const {
	if (context.isDebugCameraActive) {
		if (context.debugFlyCamera) {
			context.debugFlyCamera->SetCanUseKeyboard(context.canUseKeyboardInput);
			context.debugFlyCamera->Update();
		}
		if (context.obstacles) {
			for (auto& obstacle : *context.obstacles) {
				obstacle->Update();
			}
		}
		if (context.debugEditorController) {
			context.debugEditorController->TryPlaceEnemyAtGameViewMouse(
				context.debugFlyCamera, context.uiManager, context.simulationManager);
		}
	} else if (context.camera) {
		const bool handledByCinematic = context.cinematicCameraCoordinator && context.gameOverSequence &&
			context.bossClearSequence &&
			context.cinematicCameraCoordinator->TryUpdate(
				context.camera, context.isDebugCameraActive,
				*context.gameOverSequence,
				context.bossClearSequence->GetPhaseState(),
				context.bossClearSequence->GetTimeline(),
				context.bossClearSequence->GetFocusState(),
				context.bossClearSequence->GetCameraState(),
				BossClearSequenceController::kPhase1DurationSec,
				BossClearSequenceController::kPhase2FlashDurationSec);

		if (!handledByCinematic && context.player) {
			if (context.isAutoFlightMode) {
				const float time = static_cast<float>(context.titleBackgroundFrame) * (1.0f / 60.0f);
				context.player->UpdateCamera(context.camera, nullptr, false);
				if (context.titleLaunchState && context.titleLaunchState->IsActive()) {
					const float launchProgress = std::clamp(
						static_cast<float>(context.titleLaunchState->GetFrame()) /
						static_cast<float>(TitleLaunchState::kDurationFrames),
						0.0f, 1.0f);
					const float fovProgress = std::clamp((launchProgress - 0.16f) / 0.62f, 0.0f, 1.0f);
					const float easedProgress = fovProgress * fovProgress * (3.0f - 2.0f * fovProgress);
					const float impactProgress = std::clamp(
						1.0f - std::abs(launchProgress - 0.77f) / 0.15f, 0.0f, 1.0f);
					context.camera->SetFovY(0.60f + easedProgress * 0.62f + impactProgress * impactProgress * 0.09f);
					const Vector3 shake = CalculateTitleLaunchCameraShake(
						launchProgress, context.titleLaunchState->GetFrame());
					const Vector3 basePosition = context.camera->GetTranslate();
					context.camera->SetTranslate({
						basePosition.x + shake.x,
						basePosition.y + shake.y,
						basePosition.z + shake.z,
					});
				} else {
					context.camera->SetFovY(0.60f + std::sin(time * 0.55f) * 0.035f);
				}
			} else {
				Vector3* targetPos = nullptr;
				Vector3 enemyPos{};
				if (context.targetSelection && context.targetSelection->HasLockedEnemy() &&
					context.lockOnManager && context.lockOnManager->IsLockedEnemyAlive()) {
					enemyPos = context.lockOnManager->GetLockedTargetPosition();
					targetPos = &enemyPos;
				}
				context.player->UpdateCamera(context.camera, targetPos);
				if (context.warpExitPresentation && context.warpExitPresentation->IsActive() && !context.isDebugCameraActive) {
					context.warpExitPresentation->UpdateCamera(context.camera, context.player);
				} else if (context.cameraManager && context.cameraManager->IsCinematicLockOnEnabled() &&
					context.targetSelection && context.targetSelection->HasAliveLockedEnemy()) {
					context.cameraManager->UpdateCinematicLockOnCamera(
						context.camera, context.targetSelection->GetLockedEnemy(), context.player->GetPosition());
				} else if (context.cameraManager) {
					context.cameraManager->ResetCinematicCamera();
				}
			}
		}
		context.camera->Update();
	}

	// Object3d は Update 時のカメラ行列を WVP として保持する。確定したカメラ行列でもう一度更新する。
	if (context.obstacles) {
		for (auto& obstacle : *context.obstacles) {
			if (obstacle) {
				obstacle->Update();
			}
		}
	}
}

void GameplayUpdateCoordinator::UpdateMissilesAndHitEffects(const GameplayMissileUpdateContext& context) const {
	std::vector<Vector3> hitPositions;
	std::vector<Vector3> homingHitPositions;
	std::vector<MissileManager::ShieldImpact> shieldImpacts;
	std::vector<MissileManager::DefeatedEnemy> destroyedEnemies;

	if (context.updateSelectedMissiles) {
		if (context.missileManager && context.enemies && context.obstacles) {
			context.missileManager->Update(
				context.activeCamera, *context.enemies, *context.obstacles,
				hitPositions, homingHitPositions, shieldImpacts, destroyedEnemies);
		}

		if (context.explosionManager && !hitPositions.empty()) {
			context.explosionManager->CreateHitEffects(hitPositions);
		}
		if (context.explosionManager && !homingHitPositions.empty()) {
			context.explosionManager->CreateHomingHitEffects(homingHitPositions);
		}
		if (context.particleManager) {
			for (const MissileManager::ShieldImpact& shieldImpact : shieldImpacts) {
				// 側面シールドは本体の橙色爆発ではなく、法線方向へ青白い電気火花を散らす。
				context.particleManager->EmitDirectionalBurst("test", shieldImpact.position, shieldImpact.normal, 22,
					{ 0.42f, 0.82f, 1.0f, 0.84f }, 0.52f,
					1.55f, 0.36f, 0.038f, 0.016f, 0.12f, 0.24f, 0.10f);
				context.particleManager->EmitFireball("test", shieldImpact.position, 1,
					{ 0.78f, 0.94f, 1.0f, 0.72f }, 0.12f, 0.44f, 0.09f, 0.0f);
			}
		}
		for (const MissileManager::DefeatedEnemy& defeatedEnemy : destroyedEnemies) {
			Enemy* destroyedEnemy = defeatedEnemy.enemy;
			if (!destroyedEnemy) {
				continue;
			}

			// 通常弾では通常敵の機体を撃ち砕き、リングを出さずに破片だけを飛散させる。
			if (defeatedEnemy.projectileType == MissileType::Normal && !destroyedEnemy->IsBoss()) {
				if (context.enemyDefeatEffectController) {
					context.enemyDefeatEffectController->SpawnNormalEnemyDebris(*destroyedEnemy, context.particleManager);
				}
				// 誘導弾は敵モデルとリングを中心へ収束させて消滅させる。
			} else if (defeatedEnemy.projectileType == MissileType::MissileWithTrail && !destroyedEnemy->IsBoss()) {
				if (context.enemyDefeatEffectController) {
					context.enemyDefeatEffectController->SpawnHomingEnemyImplosion(*destroyedEnemy);
				}
				if (context.explosionManager) {
					context.explosionManager->CreateImplosionEffects({ destroyedEnemy->GetAimPosition() });
				}
			} else if (context.explosionManager) {
				// ボスは既存の演出を維持する。
				context.explosionManager->CreateDestructionEffects(
					{ destroyedEnemy->GetAimPosition() },
					ExplosionManager::ClassifyEnemy(*destroyedEnemy));
			}
		}
		if (context.enemyDefeatEffectController) {
			context.enemyDefeatEffectController->Update(context.particleManager);
		}
	} else {
		if (context.missileManager) {
			context.missileManager->UpdateModels(context.activeCamera);
		}
	}

	if ((context.shouldUpdateGame || context.isBossClearSequenceActive ||
		context.updateSelectedMissiles || context.updateSelectedParticles ||
		context.isDestructionEffectPreviewActive) && context.explosionManager) {
		context.explosionManager->Update();
	}
}
