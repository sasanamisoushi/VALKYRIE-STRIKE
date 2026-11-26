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

void GroundEnemy::TakeDamage(int damage, const Vector3& hitDirection) {
	if (!CanReceiveDamage() || damage <= 0) {
		return;
	}

	const Vector3 currentPos = GetPosition();
	const Vector3 currentForward = GetForward();
	const Vector3 currentVel = GetVelocity();

	// 弾丸の進行方向が取れない攻撃は、現在の進行方向を安全な代替に使う。
	hitDirection_ = NormalizeOrFallback(hitDirection, currentForward);
	// 中心座標ではなく弾が入射した側の装甲表面に置く。
	// これで胴体に隠れず、どの方向から撃っても火花が見える。
	const float armorRadius = std::clamp(GetCollisionRadius() * 0.62f, 0.80f, 1.45f);
	const Vector3 impactPosition = {
		currentPos.x - hitDirection_.x * armorRadius,
		currentPos.y - hitDirection_.y * armorRadius + 0.18f,
		currentPos.z - hitDirection_.z * armorRadius,
	};
	QueueEffect(EffectEvent::Type::ArmorSpark, impactPosition, hitDirection_);

	// 装甲に弾かれた反動。履帯時だけ横滑りの慣性を強める。
	hitStopFrames_ = 4;
	hitReactionFrames_ = 20;
	hitLean_ = 1.0f;
	const float side = currentForward.x * hitDirection_.z - currentForward.z * hitDirection_.x;
	skidYawVelocity_ = std::clamp(side * 0.055f, -0.055f, 0.055f);
	skidVelocity_ = {
		currentVel.x * 0.82f + hitDirection_.x * 0.045f,
		0.0f,
		currentVel.z * 0.82f + hitDirection_.z * 0.045f,
	};
	if (trackBlend_ > 0.15f) {
		QueueEffect(EffectEvent::Type::TrackSkid, currentPos, skidVelocity_);
	}

	ApplyDamage(damage);
	if (GetHP() <= 0) {
		BeginDestruction(hitDirection_);
	}
}

void GroundEnemy::UpdateHitReaction(const std::list<std::unique_ptr<Obstacle>>& obstacles) {
	MoveBy({ skidVelocity_.x, 0.0f, skidVelocity_.z });
	skidVelocity_.x *= trackBlend_ > 0.1f ? 0.91f : 0.78f;
	skidVelocity_.z *= trackBlend_ > 0.1f ? 0.91f : 0.78f;
	Vector3 rot = GetRotation();
	rot.y += skidYawVelocity_;
	SetRotationAngles(rot);
	skidYawVelocity_ *= 0.80f;
	SetForward({ std::sin(rot.y), 0.0f, std::cos(rot.y) });
	hitLean_ *= 0.87f;
	--hitReactionFrames_;
	UpdateTerrainPose(obstacles);
}

void GroundEnemy::BeginDestruction(const Vector3& hitDirection) {
	destructionState_ = DestructionState::Sliding;
	destructionFrames_ = 0;
	SetChasingPlayer(false);
	isMeleeActive_ = false;
	attackSubState_ = GroundAttackState::Idle;
	hitStopFrames_ = 0;
	hitReactionFrames_ = 0;
	hitLean_ = 1.0f;
	const Vector3 currentVel = GetVelocity();
	Vector3 nextVel = {
		currentVel.x + hitDirection.x * 0.055f,
		0.0f,
		currentVel.z + hitDirection.z * 0.055f,
	};
	if (std::abs(nextVel.x) + std::abs(nextVel.z) < 0.03f) {
		const Vector3 fwd = GetForward();
		nextVel.x = fwd.x * 0.10f;
		nextVel.z = fwd.z * 0.10f;
	}
	SetVelocity(nextVel);
	QueueEffect(EffectEvent::Type::TrackSkid, GetPosition(), nextVel);
}

void GroundEnemy::UpdateDetachedParts() {
	const float floorY = groundY_ + 0.08f;
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

	if (turretDetached_) {
		updatePart(turretDebrisPosition_, turretDebrisVelocity_, turretDebrisRotation_);
		if (turret_) {
			turret_->SetTranslate(turretDebrisPosition_);
			turret_->SetRotate(turretDebrisRotation_);
			turret_->SetScale(visualScale_);
			turret_->Update();
		}
	}
	for (size_t i = 0; i < legs_.size(); ++i) {
		if (!legDetached_[i]) continue;
		updatePart(legDebrisPositions_[i], legDebrisVelocities_[i], legDebrisRotations_[i]);
		if (legs_[i]) {
			legs_[i]->SetTranslate(legDebrisPositions_[i]);
			legs_[i]->SetRotate(legDebrisRotations_[i]);
			legs_[i]->SetScale(visualScale_);
			legs_[i]->Update();
		}
	}
}

void GroundEnemy::UpdateDestruction() {
	++destructionFrames_;
	Vector3 vel = GetVelocity();
	MoveBy({ vel.x, 0.0f, vel.z });
	vel.x *= 0.925f;
	vel.z *= 0.925f;
	SetVelocity(vel);

	Vector3 rot = GetRotation();
	rot.y += skidYawVelocity_;
	SetRotationAngles(rot);
	skidYawVelocity_ *= 0.86f;
	const Vector3 fwd = { std::sin(rot.y), 0.0f, std::cos(rot.y) };
	SetForward(fwd);

	if (destructionFrames_ == 8 && !turretDetached_) {
		turretDetached_ = true;
		turretDebrisPosition_ = GetLocalPartPosition(kTurretPivot);
		turretDebrisVelocity_ = { vel.x + hitDirection_.x * 0.13f, 0.20f, vel.z + hitDirection_.z * 0.13f };
		turretDebrisRotation_ = visualRotation_;
	}
	if (turretDetached_ && (destructionFrames_ == 13 || destructionFrames_ == 25 || destructionFrames_ == 37)) {
		const Vector3 misfireDirection = NormalizeOrFallback({ hitDirection_.z, 0.18f, -hitDirection_.x }, fwd);
		QueueEffect(EffectEvent::Type::TurretMisfire, turretDebrisPosition_, misfireDirection);
	}
	if (destructionFrames_ == 20 || destructionFrames_ == 32) {
		const size_t firstLeg = destructionFrames_ == 20 ? 0 : 2;
		for (size_t i = firstLeg; i < firstLeg + 2; ++i) {
			legDetached_[i] = true;
			legDebrisPositions_[i] = GetLocalPartPosition(kLegPivots[i]);
			const float side = (i % 2 == 0) ? 1.0f : -1.0f;
			legDebrisVelocities_[i] = { vel.x + side * 0.11f, 0.13f + static_cast<float>(i) * 0.015f, vel.z - hitDirection_.z * 0.07f };
			legDebrisRotations_[i] = visualRotation_;
		}
	}
	if (finalDestructionEffect_ == DestructionEffect::Standard &&
		(destructionFrames_ == 14 || destructionFrames_ == 27 || destructionFrames_ == 40 || destructionFrames_ == 53)) {
		destructionState_ = DestructionState::ChainDetonating;
		const int index = destructionFrames_ / 13;
		const Vector3 local = {
			index % 2 == 0 ? -0.52f : 0.70f,
			1.15f + static_cast<float>(index % 2) * 0.42f,
			index % 3 == 0 ? -0.58f : 0.54f,
		};
		QueueEffect(EffectEvent::Type::ChainDetonation, GetLocalPartPosition(local), hitDirection_);
	}

	UpdateDetachedParts();

	if (destructionFrames_ >= 66 && !finalExplosionQueued_) {
		finalExplosionQueued_ = true;
		QueueEffect(EffectEvent::Type::FinalExplosion, GetPosition(), hitDirection_, finalDestructionEffect_);
		MarkDead();
	}
}

void GroundEnemy::UpdateDamageGlow() {
	float glow = 0.0f;
	if (destructionState_ == DestructionState::ChainDetonating) {
		glow = 0.65f + 0.55f * std::abs(std::sin(static_cast<float>(destructionFrames_) * 1.9f));
	} else if (hitReactionFrames_ > 0 || hitStopFrames_ > 0) {
		glow = 0.28f + hitLean_ * 0.50f;
	}

	auto setGlow = [glow](Object3d* part) {
		if (!part) return;
		if (DirectionalLight* light = part->GetDirectionalLightData()) {
			light->color = glow > 0.0f ? Vector4{ 1.0f, 0.33f + glow * 0.22f, 0.06f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
			light->intensity = 1.0f + glow * 2.4f;
		}
	};
	setGlow(GetObject3d());
	setGlow(turret_.get());
	for (const auto& leg : legs_) setGlow(leg.get());
}
