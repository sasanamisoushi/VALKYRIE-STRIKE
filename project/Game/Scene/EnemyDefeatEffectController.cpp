#include "EnemyDefeatEffectController.h"

#include "3D/Model.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/Enemy.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/math/MyMath.h"

#include <algorithm>
#include <cmath>

void EnemyDefeatEffectController::InitializeModel() {
	ModelManager::GetInstance()->CreateBoxModel("NormalEnemyDebris");
	if (Model* model = ModelManager::GetInstance()->FindModel("NormalEnemyDebris")) {
		model->SetColor({ 0.38f, 0.64f, 0.78f, 1.0f });
	}
}

void EnemyDefeatEffectController::SpawnNormalEnemyDebris(Enemy& enemy, ParticleManager* particleManager) {
	constexpr int kPieceCount = 5;
	constexpr int kLifetimeFrames = 52;

	const Vector3 center = enemy.GetAimPosition();
	const Vector3 halfExtents = enemy.GetWorldHalfExtents();
	if (particleManager) {
		// 実弾で撃ち砕いた瞬間だけ、白芯を含む短命の橙色スパークを出す。
		particleManager->Emit("test", center, 42, { 1.0f, 0.42f, 0.06f, 1.0f },
			1.30f, 0.55f, 0.085f, 0.040f, 0.24f, 0.46f, 0.10f);
	}
	for (int index = 0; index < kPieceCount; ++index) {
		const float phase = static_cast<float>(index) * 2.39996323f;
		const float radialDistance = 0.36f + static_cast<float>(index % 3) * 0.16f;
		const float sizeVariation = 0.26f + static_cast<float>((index * 5) % 3) * 0.06f;

		NormalEnemyDebrisPiece debris;
		debris.object = std::make_unique<Object3d>();
		debris.object->Initialize(Object3dCommon::GetInstance());
		debris.object->SetModel("NormalEnemyDebris");
		debris.baseScale = {
			(std::max)(0.18f, halfExtents.x * sizeVariation),
			(std::max)(0.12f, halfExtents.y * (sizeVariation * 0.72f)),
			(std::max)(0.18f, halfExtents.z * (sizeVariation * 1.25f))
		};
		debris.rotation = { phase * 0.37f, phase, phase * 0.61f };
		debris.angularVelocity = {
			0.12f + static_cast<float>(index % 3) * 0.035f,
			0.16f + static_cast<float>(index % 5) * 0.025f,
			0.10f + static_cast<float>(index % 4) * 0.030f
		};
		debris.velocity = {
			std::cos(phase) * (0.28f + radialDistance * 0.48f),
			0.20f + static_cast<float>(index % 3) * 0.060f,
			std::sin(phase) * (0.28f + radialDistance * 0.48f)
		};
		debris.remainingFrames = kLifetimeFrames;

		debris.object->SetTranslate({
			center.x + std::cos(phase) * halfExtents.x * radialDistance,
			center.y + std::sin(phase * 1.7f) * halfExtents.y * radialDistance,
			center.z + std::sin(phase) * halfExtents.z * radialDistance
		});
		debris.object->SetScale(debris.baseScale);
		debris.object->SetRotate(debris.rotation);
		debris.object->Update();
		normalEnemyDebris_.push_back(std::move(debris));
	}
	// 本体は同フレームから描画しない。次フレームの通常撃破処理でリストからも削除される。
	enemy.SetPresentationVisible(false);
}

void EnemyDefeatEffectController::SpawnHomingEnemyImplosion(Enemy& enemy) {
	Model* model = enemy.GetPresentationModel();
	if (!model) {
		return;
	}

	HomingEnemyImplosion implosion;
	implosion.object = std::make_unique<Object3d>();
	implosion.object->Initialize(Object3dCommon::GetInstance());
	implosion.object->SetModelPointer(model);
	implosion.baseScale = enemy.GetScale();
	implosion.rotation = enemy.GetRotation();
	// The ring animation uses the same 30-frame duration in ExplosionManager.
	implosion.remainingFrames = 30;
	implosion.object->SetTranslate(enemy.GetAimPosition());
	implosion.object->SetScale(implosion.baseScale);
	implosion.object->SetRotate(implosion.rotation);
	implosion.object->Update();
	homingEnemyImplosions_.push_back(std::move(implosion));
	enemy.SetPresentationVisible(false);
}

void EnemyDefeatEffectController::Update(ParticleManager* particleManager) {
	UpdateNormalEnemyDebris();
	UpdateHomingEnemyImplosions(particleManager);
}

void EnemyDefeatEffectController::UpdateNormalEnemyDebris() {
	constexpr int kFadeFrames = 14;
	for (auto it = normalEnemyDebris_.begin(); it != normalEnemyDebris_.end();) {
		NormalEnemyDebrisPiece& debris = *it;
		if (!debris.object || --debris.remainingFrames <= 0) {
			it = normalEnemyDebris_.erase(it);
			continue;
		}

		debris.velocity.y -= 0.010f;
		Vector3 position = debris.object->GetTranslate();
		position.x += debris.velocity.x;
		position.y += debris.velocity.y;
		position.z += debris.velocity.z;
		debris.rotation.x += debris.angularVelocity.x;
		debris.rotation.y += debris.angularVelocity.y;
		debris.rotation.z += debris.angularVelocity.z;

		const float visibleScale = (std::min)(1.0f,
			static_cast<float>(debris.remainingFrames) / static_cast<float>(kFadeFrames));
		debris.object->SetTranslate(position);
		debris.object->SetRotate(debris.rotation);
		debris.object->SetScale({
			debris.baseScale.x * visibleScale,
			debris.baseScale.y * visibleScale,
			debris.baseScale.z * visibleScale
		});
		debris.object->Update();
		++it;
	}
}

void EnemyDefeatEffectController::UpdateHomingEnemyImplosions(ParticleManager* particleManager) {
	constexpr int kDurationFrames = 30;
	for (auto it = homingEnemyImplosions_.begin(); it != homingEnemyImplosions_.end();) {
		HomingEnemyImplosion& implosion = *it;
		if (!implosion.object || --implosion.remainingFrames <= 0) {
			if (implosion.object && particleManager) {
				// 収束直後に白い高熱フラッシュを一度だけ広げて消す。
				particleManager->EmitFireball("test", implosion.object->GetTranslate(), 1,
					{ 1.0f, 1.0f, 1.0f, 0.90f }, 0.50f, 4.0f, 0.15f, 0.0f);
			}
			it = homingEnemyImplosions_.erase(it);
			continue;
		}

		const float progress = 1.0f - static_cast<float>(implosion.remainingFrames) /
			static_cast<float>(kDurationFrames);
		const float scale = (1.0f - progress) * (1.0f - progress);
		implosion.rotation.x += 0.055f;
		implosion.rotation.y += 0.180f;
		implosion.rotation.z += 0.090f;
		implosion.object->SetScale({
			implosion.baseScale.x * scale,
			implosion.baseScale.y * scale,
			implosion.baseScale.z * scale
		});
		implosion.object->SetRotate(implosion.rotation);
		implosion.object->Update();
		++it;
	}
}

void EnemyDefeatEffectController::Draw(const Vector4 frustumPlanes[6]) const {
	for (const NormalEnemyDebrisPiece& debris : normalEnemyDebris_) {
		if (!debris.object) {
			continue;
		}
		Sphere debrisSphere;
		debrisSphere.center = debris.object->GetTranslate();
		debrisSphere.radius = MyMath::Length(debris.object->GetScale());
		if (MyMath::IsInFrustum(debrisSphere, frustumPlanes)) {
			debris.object->Draw();
		}
	}
	for (const HomingEnemyImplosion& implosion : homingEnemyImplosions_) {
		if (!implosion.object) {
			continue;
		}
		Sphere implosionSphere;
		implosionSphere.center = implosion.object->GetTranslate();
		implosionSphere.radius = MyMath::Length(implosion.object->GetScale());
		if (MyMath::IsInFrustum(implosionSphere, frustumPlanes)) {
			implosion.object->Draw();
		}
	}
}

void EnemyDefeatEffectController::Clear() {
	normalEnemyDebris_.clear();
	homingEnemyImplosions_.clear();
}
