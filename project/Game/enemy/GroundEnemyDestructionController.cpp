#include "GroundEnemyDestructionController.h"

#include "GroundEnemy.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr Vector3 kTurretPivot = { 0.45f, 2.18f, 0.0f };
constexpr std::array<Vector3, 4> kLegPivots = {
	Vector3{ -0.62f, 1.35f, 0.76f }, Vector3{ -0.62f, 1.35f, -0.76f },
	Vector3{ 0.80f, 1.35f, -0.76f }, Vector3{ 0.80f, 1.35f, 0.76f }
};

Vector3 NormalizeOrFallback(const Vector3& v, const Vector3& fallback) {
	float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
	if (lenSq <= 0.0001f) {
		return fallback;
	}
	float len = std::sqrt(lenSq);
	return { v.x / len, v.y / len, v.z / len };
}
}

void GroundEnemyDestructionController::TakeDamage(
	GroundEnemy& enemy,
	int damage,
	const Vector3& hitDirection) const {
	if (!enemy.CanReceiveDamage() || damage <= 0) {
		return;
	}

	// 弾丸の進行方向が取れない攻撃は、現在の進行方向を安全な代替に使う。
	enemy.hitDirection_ = NormalizeOrFallback(hitDirection, enemy.forward_);
	// 中心座標ではなく弾が入射した側の装甲表面に置く。
	// これで胴体に隠れず、どの方向から撃っても火花が見える。
	const float armorRadius = std::clamp(enemy.GetCollisionRadius() * 0.62f, 0.80f, 1.45f);
	const Vector3 impactPosition = {
		enemy.position_.x - enemy.hitDirection_.x * armorRadius,
		enemy.position_.y - enemy.hitDirection_.y * armorRadius + 0.18f,
		enemy.position_.z - enemy.hitDirection_.z * armorRadius,
	};
	enemy.QueueEffect(GroundEnemy::EffectEvent::Type::ArmorSpark, impactPosition, enemy.hitDirection_);

	// 装甲に弾かれた反動。履帯時だけ横滑りの慣性を強める。
	enemy.hitStopFrames_ = 4;
	enemy.hitReactionFrames_ = 20;
	enemy.hitLean_ = 1.0f;
	const float side = enemy.forward_.x * enemy.hitDirection_.z - enemy.forward_.z * enemy.hitDirection_.x;
	enemy.skidYawVelocity_ = std::clamp(side * 0.055f, -0.055f, 0.055f);
	enemy.skidVelocity_ = {
		enemy.velocity_.x * 0.82f + enemy.hitDirection_.x * 0.045f,
		0.0f,
		enemy.velocity_.z * 0.82f + enemy.hitDirection_.z * 0.045f,
	};
	if (enemy.trackBlend_ > 0.15f) {
		enemy.QueueEffect(GroundEnemy::EffectEvent::Type::TrackSkid, enemy.position_, enemy.skidVelocity_);
	}

	enemy.hp_ -= damage;
	if (enemy.hp_ <= 0) {
		enemy.hp_ = 0;
		BeginDestruction(enemy, enemy.hitDirection_);
	}
}

void GroundEnemyDestructionController::UpdateHitReaction(
	GroundEnemy& enemy,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	enemy.position_.x += enemy.skidVelocity_.x;
	enemy.position_.z += enemy.skidVelocity_.z;
	enemy.skidVelocity_.x *= enemy.trackBlend_ > 0.1f ? 0.91f : 0.78f;
	enemy.skidVelocity_.z *= enemy.trackBlend_ > 0.1f ? 0.91f : 0.78f;
	enemy.rotation_.y += enemy.skidYawVelocity_;
	enemy.skidYawVelocity_ *= 0.80f;
	enemy.forward_ = { std::sin(enemy.rotation_.y), 0.0f, std::cos(enemy.rotation_.y) };
	enemy.hitLean_ *= 0.87f;
	--enemy.hitReactionFrames_;
	enemy.UpdateTerrainPose(obstacles);
}

void GroundEnemyDestructionController::BeginDestruction(GroundEnemy& enemy, const Vector3& hitDirection) const {
	enemy.destructionState_ = GroundEnemy::DestructionState::Sliding;
	enemy.destructionFrames_ = 0;
	enemy.isChasingPlayer_ = false;
	enemy.isMeleeActive_ = false;
	enemy.attackSubState_ = GroundEnemy::GroundAttackState::Idle;
	enemy.hitStopFrames_ = 0;
	enemy.hitReactionFrames_ = 0;
	enemy.hitLean_ = 1.0f;
	enemy.velocity_ = {
		enemy.velocity_.x + hitDirection.x * 0.055f,
		0.0f,
		enemy.velocity_.z + hitDirection.z * 0.055f,
	};
	if (std::abs(enemy.velocity_.x) + std::abs(enemy.velocity_.z) < 0.03f) {
		enemy.velocity_.x = enemy.forward_.x * 0.10f;
		enemy.velocity_.z = enemy.forward_.z * 0.10f;
	}
	enemy.QueueEffect(GroundEnemy::EffectEvent::Type::TrackSkid, enemy.position_, enemy.velocity_);
}

void GroundEnemyDestructionController::UpdateDetachedParts(GroundEnemy& enemy) const {
	const float floorY = enemy.groundY_ + 0.08f;
	auto updatePart = [floorY](Vector3& partPosition, Vector3& partVelocity, Vector3& partRotation) {
		partVelocity.y -= 0.018f;
		partPosition.x += partVelocity.x;
		partPosition.y += partVelocity.y;
		partPosition.z += partVelocity.z;
		partRotation.x += 0.10f;
		partRotation.y += 0.15f;
		partRotation.z += 0.07f;
		if (partPosition.y < floorY) {
			partPosition.y = floorY;
			partVelocity.y = -partVelocity.y * 0.24f;
			partVelocity.x *= 0.79f;
			partVelocity.z *= 0.79f;
		}
	};

	if (enemy.turretDetached_) {
		updatePart(enemy.turretDebrisPosition_, enemy.turretDebrisVelocity_, enemy.turretDebrisRotation_);
		if (enemy.turret_) {
			enemy.turret_->SetTranslate(enemy.turretDebrisPosition_);
			enemy.turret_->SetRotate(enemy.turretDebrisRotation_);
			enemy.turret_->SetScale(enemy.visualScale_);
			enemy.turret_->Update();
		}
	}
	for (size_t i = 0; i < enemy.legs_.size(); ++i) {
		if (!enemy.legDetached_[i]) continue;
		updatePart(enemy.legDebrisPositions_[i], enemy.legDebrisVelocities_[i], enemy.legDebrisRotations_[i]);
		if (enemy.legs_[i]) {
			enemy.legs_[i]->SetTranslate(enemy.legDebrisPositions_[i]);
			enemy.legs_[i]->SetRotate(enemy.legDebrisRotations_[i]);
			enemy.legs_[i]->SetScale(enemy.visualScale_);
			enemy.legs_[i]->Update();
		}
	}
}

void GroundEnemyDestructionController::UpdateDestruction(GroundEnemy& enemy) const {
	++enemy.destructionFrames_;
	enemy.position_.x += enemy.velocity_.x;
	enemy.position_.z += enemy.velocity_.z;
	enemy.velocity_.x *= 0.925f;
	enemy.velocity_.z *= 0.925f;
	enemy.rotation_.y += enemy.skidYawVelocity_;
	enemy.skidYawVelocity_ *= 0.86f;
	enemy.forward_ = { std::sin(enemy.rotation_.y), 0.0f, std::cos(enemy.rotation_.y) };

	if (enemy.destructionFrames_ == 8 && !enemy.turretDetached_) {
		enemy.turretDetached_ = true;
		enemy.turretDebrisPosition_ = enemy.GetLocalPartPosition(kTurretPivot);
		enemy.turretDebrisVelocity_ = { enemy.velocity_.x + enemy.hitDirection_.x * 0.13f, 0.20f, enemy.velocity_.z + enemy.hitDirection_.z * 0.13f };
		enemy.turretDebrisRotation_ = enemy.visualRotation_;
	}
	if (enemy.turretDetached_ && (enemy.destructionFrames_ == 13 || enemy.destructionFrames_ == 25 || enemy.destructionFrames_ == 37)) {
		const Vector3 misfireDirection = NormalizeOrFallback({ enemy.hitDirection_.z, 0.18f, -enemy.hitDirection_.x }, enemy.forward_);
		enemy.QueueEffect(GroundEnemy::EffectEvent::Type::TurretMisfire, enemy.turretDebrisPosition_, misfireDirection);
	}
	if (enemy.destructionFrames_ == 20 || enemy.destructionFrames_ == 32) {
		const size_t firstLeg = enemy.destructionFrames_ == 20 ? 0 : 2;
		for (size_t i = firstLeg; i < firstLeg + 2; ++i) {
			enemy.legDetached_[i] = true;
			enemy.legDebrisPositions_[i] = enemy.GetLocalPartPosition(kLegPivots[i]);
			const float side = (i % 2 == 0) ? 1.0f : -1.0f;
			enemy.legDebrisVelocities_[i] = { enemy.velocity_.x + side * 0.11f, 0.13f + static_cast<float>(i) * 0.015f, enemy.velocity_.z - enemy.hitDirection_.z * 0.07f };
			enemy.legDebrisRotations_[i] = enemy.visualRotation_;
		}
	}
	if (enemy.finalDestructionEffect_ == GroundEnemy::DestructionEffect::Standard &&
		(enemy.destructionFrames_ == 14 || enemy.destructionFrames_ == 27 || enemy.destructionFrames_ == 40 || enemy.destructionFrames_ == 53)) {
		enemy.destructionState_ = GroundEnemy::DestructionState::ChainDetonating;
		const int index = enemy.destructionFrames_ / 13;
		const Vector3 local = {
			index % 2 == 0 ? -0.52f : 0.70f,
			1.15f + static_cast<float>(index % 2) * 0.42f,
			index % 3 == 0 ? -0.58f : 0.54f,
		};
		enemy.QueueEffect(GroundEnemy::EffectEvent::Type::ChainDetonation, enemy.GetLocalPartPosition(local), enemy.hitDirection_);
	}

	UpdateDetachedParts(enemy);

	if (enemy.destructionFrames_ >= 66 && !enemy.finalExplosionQueued_) {
		enemy.finalExplosionQueued_ = true;
		enemy.QueueEffect(GroundEnemy::EffectEvent::Type::FinalExplosion, enemy.position_, enemy.hitDirection_, enemy.finalDestructionEffect_);
		enemy.isDead_ = true;
	}
}

void GroundEnemyDestructionController::UpdateDamageGlow(GroundEnemy& enemy) const {
	float glow = 0.0f;
	if (enemy.destructionState_ == GroundEnemy::DestructionState::ChainDetonating) {
		glow = 0.65f + 0.55f * std::abs(std::sin(static_cast<float>(enemy.destructionFrames_) * 1.9f));
	} else if (enemy.hitReactionFrames_ > 0 || enemy.hitStopFrames_ > 0) {
		glow = 0.28f + enemy.hitLean_ * 0.50f;
	}

	auto setGlow = [glow](Object3d* part) {
		if (!part) return;
		if (DirectionalLight* light = part->GetDirectionalLightData()) {
			light->color = glow > 0.0f ? Vector4{ 1.0f, 0.33f + glow * 0.22f, 0.06f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
			light->intensity = 1.0f + glow * 2.4f;
		}
	};
	setGlow(enemy.object_.get());
	setGlow(enemy.turret_.get());
	for (const auto& leg : enemy.legs_) setGlow(leg.get());
}
