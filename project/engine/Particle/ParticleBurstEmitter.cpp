#include "ParticleBurstEmitter.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
uint32_t ClampEmitCountToCapacity(const ParticleManager::ParticleGroup& group, uint32_t requestedCount) {
	const size_t maxCount = static_cast<size_t>(group.kNumMaxInstance);
	const size_t activeCount = group.particle.size();
	if (activeCount >= maxCount) {
		return 0;
	}

	const size_t availableCount = maxCount - activeCount;
	return static_cast<uint32_t>((std::min)(static_cast<size_t>(requestedCount), availableCount));
}
}

void ParticleBurstEmitter::Emit(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count, const Vector4& color,
	float speed, float speedVariance, float scale, float scaleVariance,
	float lifeTimeMin, float lifeTimeMax, float posVariance) const {
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	static std::random_device seed_gen;
	static std::mt19937_64 engine(seed_gen());

	constexpr float kPi = 3.14159265f;
	std::uniform_real_distribution<float> distZ(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distRotate(-3.14159265f, 3.14159265f);
	std::uniform_real_distribution<float> distAngularSpeed(0.06f, 0.16f);
	std::uniform_int_distribution<int> distSpinDirection(0, 1);
	std::uniform_real_distribution<float> distScale(scale - scaleVariance, scale + scaleVariance);
	std::uniform_real_distribution<float> distRadius(posVariance * 0.4f, posVariance);
	std::uniform_real_distribution<float> distLife(lifeTimeMin, lifeTimeMax);
	std::uniform_real_distribution<float> distSpeed(speed - speedVariance, speed + speedVariance);

	for (uint32_t i = 0; i < emitCount; ++i) {
		ParticleManager::Particle newParticle{};

		float randomScale = distScale(engine);
		newParticle.initialScale = randomScale;
		newParticle.finalScale = 0.0f;
		newParticle.transform.scale = { randomScale, randomScale, 1.0f };

		newParticle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		const float spinDirection = distSpinDirection(engine) == 0 ? -1.0f : 1.0f;
		newParticle.angularVelocity = distAngularSpeed(engine) * spinDirection;

		const float z = distZ(engine);
		const float angle = distAngle(engine);
		const float xyLength = std::sqrt(1.0f - z * z);
		const Vector3 dir = {
			xyLength * std::cos(angle),
			xyLength * std::sin(angle),
			z
		};
		const float spawnRadius = distRadius(engine);
		newParticle.transform.translate = {
			position.x + dir.x * spawnRadius,
			position.y + dir.y * spawnRadius,
			position.z + dir.z * spawnRadius
		};

		float kSpeed = distSpeed(engine);
		newParticle.velocity = {
			dir.x * kSpeed,
			dir.y * kSpeed,
			dir.z * kSpeed
		};

		newParticle.color = color;
		newParticle.lifeTime = distLife(engine);
		newParticle.currentTime = 0.0f;

		group.particle.push_back(newParticle);
	}
}

void ParticleBurstEmitter::EmitDirectionalBurst(ParticleManager::ParticleGroup& group, const Vector3& position, const Vector3& direction,
	uint32_t count, const Vector4& color, float spreadRadians,
	float speed, float speedVariance, float scale, float scaleVariance,
	float lifeTimeMin, float lifeTimeMax, float posVariance) const {
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	const float directionLength = std::sqrt(
		direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
	if (directionLength <= 0.0001f) {
		return;
	}

	const Vector3 forward = {
		direction.x / directionLength,
		direction.y / directionLength,
		direction.z / directionLength,
	};
	const Vector3 referenceAxis = std::abs(forward.y) > 0.92f
		? Vector3{ 1.0f, 0.0f, 0.0f }
		: Vector3{ 0.0f, 1.0f, 0.0f };
	Vector3 right = {
		referenceAxis.y * forward.z - referenceAxis.z * forward.y,
		referenceAxis.z * forward.x - referenceAxis.x * forward.z,
		referenceAxis.x * forward.y - referenceAxis.y * forward.x,
	};
	const float rightLength = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
	right = { right.x / rightLength, right.y / rightLength, right.z / rightLength };
	const Vector3 up = {
		forward.y * right.z - forward.z * right.y,
		forward.z * right.x - forward.x * right.z,
		forward.x * right.y - forward.y * right.x,
	};

	static std::random_device seedGenerator;
	static std::mt19937_64 engine(seedGenerator());
	constexpr float kPi = 3.14159265f;
	const float clampedSpread = std::clamp(spreadRadians, 0.0f, 1.50f);
	std::uniform_real_distribution<float> distCosTheta(std::cos(clampedSpread), 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distSpeed(speed - speedVariance, speed + speedVariance);
	std::uniform_real_distribution<float> distScale(scale - scaleVariance, scale + scaleVariance);
	std::uniform_real_distribution<float> distLife(lifeTimeMin, lifeTimeMax);
	std::uniform_real_distribution<float> distRadius(0.0f, posVariance);
	std::uniform_real_distribution<float> distRotate(-kPi, kPi);
	std::uniform_real_distribution<float> distAngularSpeed(0.08f, 0.22f);
	std::uniform_int_distribution<int> distSpinDirection(0, 1);

	for (uint32_t i = 0; i < emitCount; ++i) {
		const float cosTheta = distCosTheta(engine);
		const float sinTheta = std::sqrt((std::max)(0.0f, 1.0f - cosTheta * cosTheta));
		const float angle = distAngle(engine);
		const Vector3 burstDirection = {
			forward.x * cosTheta + right.x * std::cos(angle) * sinTheta + up.x * std::sin(angle) * sinTheta,
			forward.y * cosTheta + right.y * std::cos(angle) * sinTheta + up.y * std::sin(angle) * sinTheta,
			forward.z * cosTheta + right.z * std::cos(angle) * sinTheta + up.z * std::sin(angle) * sinTheta,
		};

		ParticleManager::Particle particle{};
		const float particleScale = (std::max)(0.0f, distScale(engine));
		particle.initialScale = particleScale;
		particle.finalScale = 0.0f;
		particle.transform.scale = { particleScale, particleScale, 1.0f };
		particle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		particle.angularVelocity = distAngularSpeed(engine) * (distSpinDirection(engine) == 0 ? -1.0f : 1.0f);

		const float spawnRadius = distRadius(engine);
		particle.transform.translate = {
			position.x + burstDirection.x * spawnRadius,
			position.y + burstDirection.y * spawnRadius,
			position.z + burstDirection.z * spawnRadius,
		};
		const float particleSpeed = (std::max)(0.0f, distSpeed(engine));
		particle.velocity = {
			burstDirection.x * particleSpeed,
			burstDirection.y * particleSpeed,
			burstDirection.z * particleSpeed,
		};
		particle.color = color;
		particle.lifeTime = (std::max)(0.01f, distLife(engine));
		particle.currentTime = 0.0f;
		group.particle.push_back(particle);
	}
}

void ParticleBurstEmitter::EmitInwardBurst(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count,
	const Vector4& color, float innerRadius, float outerRadius,
	float speed, float speedVariance, float scale, float scaleVariance,
	float lifeTimeMin, float lifeTimeMax) const {
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	static std::random_device seedGenerator;
	static std::mt19937_64 engine(seedGenerator());
	constexpr float kPi = 3.14159265f;
	const float minimumRadius = (std::max)(0.0f, (std::min)(innerRadius, outerRadius));
	const float maximumRadius = (std::max)(minimumRadius, (std::max)(innerRadius, outerRadius));
	std::uniform_real_distribution<float> distZ(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distRadius(minimumRadius, maximumRadius);
	std::uniform_real_distribution<float> distSpeed((std::max)(0.0f, speed - speedVariance),
		(std::max)(0.0f, speed + speedVariance));
	std::uniform_real_distribution<float> distScale((std::max)(0.0f, scale - scaleVariance),
		(std::max)(0.0f, scale + scaleVariance));
	std::uniform_real_distribution<float> distLife((std::max)(0.01f, lifeTimeMin),
		(std::max)((std::max)(0.01f, lifeTimeMin), lifeTimeMax));
	std::uniform_real_distribution<float> distRotate(-kPi, kPi);
	std::uniform_real_distribution<float> distAngularSpeed(0.08f, 0.22f);
	std::uniform_int_distribution<int> distSpinDirection(0, 1);

	for (uint32_t i = 0; i < emitCount; ++i) {
		const float z = distZ(engine);
		const float angle = distAngle(engine);
		const float xyLength = std::sqrt((std::max)(0.0f, 1.0f - z * z));
		const Vector3 outwardDirection = {
			xyLength * std::cos(angle), xyLength * std::sin(angle), z
		};
		const float spawnRadius = distRadius(engine);
		const float particleScale = distScale(engine);
		ParticleManager::Particle particle{};
		particle.initialScale = particleScale;
		particle.finalScale = 0.0f;
		particle.transform.scale = { particleScale, particleScale, 1.0f };
		particle.transform.translate = {
			position.x + outwardDirection.x * spawnRadius,
			position.y + outwardDirection.y * spawnRadius,
			position.z + outwardDirection.z * spawnRadius,
		};
		const float particleSpeed = distSpeed(engine);
		particle.velocity = {
			-outwardDirection.x * particleSpeed,
			-outwardDirection.y * particleSpeed,
			-outwardDirection.z * particleSpeed,
		};
		particle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		particle.angularVelocity = distAngularSpeed(engine) * (distSpinDirection(engine) == 0 ? -1.0f : 1.0f);
		particle.color = color;
		particle.lifeTime = distLife(engine);
		particle.currentTime = 0.0f;
		group.particle.push_back(particle);
	}
}

void ParticleBurstEmitter::EmitSmoke(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count, const Vector4& color,
	float initialScale, float finalScale, float riseSpeed, float spreadSpeed,
	float lifeTimeMin, float lifeTimeMax, float posVariance) const {
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	static std::random_device seedGenerator;
	static std::mt19937_64 engine(seedGenerator());
	constexpr float kPi = 3.14159265f;
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distSigned(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distLife(lifeTimeMin, lifeTimeMax);
	std::uniform_real_distribution<float> distScaleJitter(0.85f, 1.20f);
	std::uniform_real_distribution<float> distRadius(0.0f, posVariance);
	std::uniform_real_distribution<float> distRotate(-kPi, kPi);
	std::uniform_real_distribution<float> distAngularSpeed(0.015f, 0.045f);

	for (uint32_t i = 0; i < emitCount; ++i) {
		ParticleManager::Particle particle{};
		const float jitter = distScaleJitter(engine);
		particle.initialScale = (std::max)(0.05f, initialScale * jitter);
		particle.finalScale = (std::max)(particle.initialScale, finalScale * jitter);
		particle.transform.scale = { particle.initialScale, particle.initialScale, 1.0f };
		particle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		particle.angularVelocity = distAngularSpeed(engine) * (distSigned(engine) < 0.0f ? -1.0f : 1.0f);

		const float theta = distAngle(engine);
		const float offsetR = distRadius(engine);
		particle.transform.translate = {
			position.x + std::cos(theta) * offsetR,
			position.y + distSigned(engine) * offsetR * 0.4f,
			position.z + std::sin(theta) * offsetR,
		};
		particle.velocity = {
			distSigned(engine) * spreadSpeed,
			riseSpeed * (0.75f + 0.5f * (distSigned(engine) * 0.5f + 0.5f)),
			distSigned(engine) * spreadSpeed,
		};
		particle.color = color;
		particle.lifeTime = (std::max)(0.05f, distLife(engine));
		particle.currentTime = 0.0f;
		group.particle.push_back(particle);
	}
}

void ParticleBurstEmitter::EmitFireball(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count,
	const Vector4& color, float initialScale, float finalScale, float lifeTime, float radius) const {
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	static std::random_device seedGenerator;
	static std::mt19937_64 engine(seedGenerator());

	constexpr float kPi = 3.14159265f;
	std::uniform_real_distribution<float> distZ(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distRadius(0.0f, radius);
	std::uniform_real_distribution<float> distScale(0.85f, 1.15f);
	std::uniform_real_distribution<float> distLife(0.9f, 1.1f);
	std::uniform_real_distribution<float> distRotate(-kPi, kPi);
	std::uniform_real_distribution<float> distAngularSpeed(0.04f, 0.11f);
	std::uniform_int_distribution<int> distSpinDirection(0, 1);

	for (uint32_t i = 0; i < emitCount; ++i) {
		const float z = distZ(engine);
		const float angle = distAngle(engine);
		const float xyLength = std::sqrt(1.0f - z * z);
		const Vector3 dir = {
			xyLength * std::cos(angle),
			xyLength * std::sin(angle),
			z
		};
		const float spawnRadius = distRadius(engine);
		const float scaleFactor = distScale(engine);

		ParticleManager::Particle particle{};
		particle.transform.translate = {
			position.x + dir.x * spawnRadius,
			position.y + dir.y * spawnRadius,
			position.z + dir.z * spawnRadius
		};
		particle.velocity = { dir.x * 0.004f, dir.y * 0.004f, dir.z * 0.004f };
		particle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		particle.angularVelocity =
			distAngularSpeed(engine) * (distSpinDirection(engine) == 0 ? -1.0f : 1.0f);
		particle.initialScale = initialScale * scaleFactor;
		particle.finalScale = finalScale * scaleFactor;
		particle.transform.scale = { particle.initialScale, particle.initialScale, 1.0f };
		particle.color = color;
		particle.lifeTime = lifeTime * distLife(engine);
		particle.currentTime = 0.0f;

		group.particle.push_back(particle);
	}
}

void ParticleBurstEmitter::EmitFlameToSmoke(ParticleManager::ParticleGroup& group, const Vector3& position, uint32_t count,
	const Vector4& flameColor, const Vector4& smokeColor,
	float initialScale, float finalScale, float speed, float speedVariance,
	float lifeTimeMin, float lifeTimeMax, float posVariance, float transitionStart) const {
	const uint32_t emitCount = ClampEmitCountToCapacity(group, count);
	if (emitCount == 0) {
		return;
	}

	static std::random_device seedGenerator;
	static std::mt19937_64 engine(seedGenerator());
	constexpr float kPi = 3.14159265f;
	std::uniform_real_distribution<float> distZ(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, kPi * 2.0f);
	std::uniform_real_distribution<float> distRadius(0.0f, (std::max)(0.0f, posVariance));
	std::uniform_real_distribution<float> distScale(0.82f, 1.18f);
	std::uniform_real_distribution<float> distSpeed((std::max)(0.0f, speed - speedVariance), (std::max)(0.0f, speed + speedVariance));
	std::uniform_real_distribution<float> distLife((std::max)(0.05f, lifeTimeMin), (std::max)(0.05f, lifeTimeMax));
	std::uniform_real_distribution<float> distRotate(-kPi, kPi);
	std::uniform_real_distribution<float> distAngularSpeed(0.018f, 0.065f);

	for (uint32_t i = 0; i < emitCount; ++i) {
		const float z = distZ(engine);
		const float angle = distAngle(engine);
		const float xyLength = std::sqrt((std::max)(0.0f, 1.0f - z * z));
		const Vector3 direction = { xyLength * std::cos(angle), z, xyLength * std::sin(angle) };
		const float scaleFactor = distScale(engine);
		const float spawnRadius = distRadius(engine);

		ParticleManager::Particle particle{};
		particle.transform.translate = {
			position.x + direction.x * spawnRadius,
			position.y + direction.y * spawnRadius,
			position.z + direction.z * spawnRadius,
		};
		const float particleSpeed = distSpeed(engine);
		particle.velocity = {
			direction.x * particleSpeed,
			direction.y * particleSpeed + 0.012f,
			direction.z * particleSpeed,
		};
		particle.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
		particle.angularVelocity = distAngularSpeed(engine);
		particle.initialScale = (std::max)(0.01f, initialScale * scaleFactor);
		particle.finalScale = (std::max)(particle.initialScale, finalScale * scaleFactor);
		particle.transform.scale = { particle.initialScale, particle.initialScale, 1.0f };
		particle.color = flameColor;
		particle.startColor = flameColor;
		particle.endColor = smokeColor;
		particle.colorTransitionStart = (std::clamp)(transitionStart, 0.0f, 0.95f);
		particle.velocityDrag = 0.040f;
		particle.upwardAcceleration = 0.0016f;
		particle.useColorTransition = true;
		particle.lifeTime = distLife(engine);
		particle.currentTime = 0.0f;
		group.particle.push_back(particle);
	}
}

void ParticleBurstEmitter::EmitCustom(ParticleManager::ParticleGroup& group, const ParticleManager::CustomParticleParams& params) const {
	if (ClampEmitCountToCapacity(group, 1) == 0) {
		return;
	}

	ParticleManager::Particle particle{};
	particle.transform.translate = params.position;
	particle.transform.rotate = { 0.0f, 0.0f, params.initialRotation };
	particle.initialScale = (std::max)(0.01f, params.initialScale);
	particle.finalScale = (std::max)(0.0f, params.finalScale);
	particle.transform.scale = { particle.initialScale, particle.initialScale, 1.0f };
	particle.velocity = params.velocity;
	particle.angularVelocity = params.angularVelocity;
	particle.color = params.color;
	particle.velocityDrag = (std::clamp)(params.velocityDrag, 0.0f, 0.95f);
	particle.upwardAcceleration = params.upwardAcceleration;
	particle.useColorTransition = false;
	particle.lifeTime = (std::max)(0.01f, params.lifeTime);
	particle.currentTime = 0.0f;
	group.particle.push_back(particle);
}
