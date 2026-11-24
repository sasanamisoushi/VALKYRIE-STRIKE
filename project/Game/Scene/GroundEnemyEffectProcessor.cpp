#include "GroundEnemyEffectProcessor.h"

#include "Game/enemy/GroundEnemy.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/Particle/ParticleManager.h"

void GroundEnemyEffectProcessor::Process(GroundEnemy& enemy, ExplosionManager* explosionManager,
	ParticleManager* particleManager) const {
	for (const GroundEnemy::EffectEvent& effect : enemy.ConsumeEffectEvents()) {
		switch (effect.type) {
		case GroundEnemy::EffectEvent::Type::ArmorSpark:
			if (explosionManager) {
				explosionManager->CreateHitEffects({ effect.position });
			}
			if (particleManager) {
				particleManager->EmitDirectionalBurst("test", effect.position, effect.direction, 72,
					{ 1.0f, 0.78f, 0.24f, 1.0f }, 0.50f,
					0.92f, 0.30f, 0.068f, 0.030f, 0.24f, 0.52f, 0.19f);
			}
			break;
		case GroundEnemy::EffectEvent::Type::TrackSkid:
			if (particleManager) {
				particleManager->EmitDirectionalBurst("smoke", effect.position, effect.direction, 24,
					{ 0.62f, 0.42f, 0.20f, 0.72f }, 0.82f,
					0.19f, 0.09f, 0.13f, 0.06f, 0.36f, 0.72f, 0.55f);
				particleManager->EmitDirectionalBurst("test", effect.position, effect.direction, 14,
					{ 1.0f, 0.34f, 0.05f, 0.92f }, 0.55f,
					0.33f, 0.12f, 0.032f, 0.014f, 0.16f, 0.32f, 0.30f);
			}
			break;
		case GroundEnemy::EffectEvent::Type::ChainDetonation:
			if (explosionManager) {
				explosionManager->CreateHitEffects({ effect.position });
			}
			break;
		case GroundEnemy::EffectEvent::Type::TurretMisfire:
			if (particleManager) {
				particleManager->EmitDirectionalBurst("test", effect.position, effect.direction, 30,
					{ 0.32f, 0.86f, 1.0f, 1.0f }, 0.18f,
					1.05f, 0.16f, 0.045f, 0.018f, 0.12f, 0.23f, 0.08f);
			}
			break;
		case GroundEnemy::EffectEvent::Type::FinalExplosion:
			if (explosionManager) {
				explosionManager->CreateDestructionEffects(
					{ effect.position }, ExplosionManager::EffectTarget::GroundEnemy);
			}
			if (particleManager) {
				particleManager->EmitDirectionalBurst("smoke", effect.position, { 0.0f, 1.0f, 0.0f }, 64,
					{ 0.28f, 0.20f, 0.14f, 0.90f }, 1.30f,
					0.55f, 0.22f, 0.22f, 0.10f, 0.55f, 1.10f, 0.80f);
			}
			break;
		}
	}
}
