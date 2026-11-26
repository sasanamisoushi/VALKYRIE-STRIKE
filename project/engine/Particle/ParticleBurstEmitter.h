#pragma once

#include "ParticleManager.h"
#include <cstdint>
#include <string>

// パーティクル各種バースト（通常・円錐・収束・煙・火球・炎→煙・カスタム）の生成を担当する。
class ParticleBurstEmitter {
public:
	void Emit(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count, const Vector4& color,
		float speed, float speedVariance, float scale, float scaleVariance,
		float lifeTimeMin, float lifeTimeMax, float posVariance) const;
	void EmitDirectionalBurst(ParticleManager::ParticleGroup& group, const Vector3& position, const Vector3& direction,
		uint32_t count, const Vector4& color, float spreadRadians,
		float speed, float speedVariance, float scale, float scaleVariance,
		float lifeTimeMin, float lifeTimeMax, float posVariance) const;
	void EmitInwardBurst(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count,
		const Vector4& color, float innerRadius, float outerRadius,
		float speed, float speedVariance, float scale, float scaleVariance,
		float lifeTimeMin, float lifeTimeMax) const;
	void EmitSmoke(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count, const Vector4& color,
		float initialScale, float finalScale, float riseSpeed, float spreadSpeed,
		float lifeTimeMin, float lifeTimeMax, float posVariance) const;
	void EmitFireball(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count,
		const Vector4& color, float initialScale, float finalScale, float lifeTime, float radius) const;
	void EmitFlameToSmoke(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count,
		const Vector4& flameColor, const Vector4& smokeColor,
		float initialScale, float finalScale, float speed, float speedVariance,
		float lifeTimeMin, float lifeTimeMax, float posVariance, float transitionStart) const;
	void EmitCustom(ParticleManager::ParticleGroup& group, const ParticleManager::CustomParticleParams& params) const;
};
