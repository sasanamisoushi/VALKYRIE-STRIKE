#include "PlayerFlightEffectController.h"

#include "DamageFeedbackState.h"
#include "GameplayBalanceSettingsManager.h"
#include "SongFlightTrailRenderer.h"
#include "SongState.h"
#include "WarpExitPresentationState.h"
#include "Game/Player/Player.h"
#include "engine/Camera/Camera.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/math/MyMath.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr char kSpeedRushParticleGroup[] = "lowAltitudeSpeedRush";

Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 ScaleVector3(const Vector3& v, float scale) {
	return { v.x * scale, v.y * scale, v.z * scale };
}

Vector3 NormalizeOrVector3(const Vector3& v, const Vector3& fallback) {
	const float len = MyMath::Length(v);
	if (len <= 0.0001f) {
		return fallback;
	}
	return { v.x / len, v.y / len, v.z / len };
}
} // namespace

void PlayerFlightEffectController::UpdatePostEffect(const Player* player, DamageFeedbackState& damageFeedbackState) const {
	if (!PostEffect::GetInstance()) {
		return;
	}

	bool isBoosting = false;
	if (player && player->GetCurrentMode() == PlayerMode::Fighter) {
		const float maxSpeed = player->GetModeParams(PlayerMode::Fighter).maxMoveSpeed;
		const float speed = MyMath::Length(player->GetVelocity());
		if (speed > maxSpeed * 1.5f) {
			isBoosting = true;
			const float effectProgress = std::clamp((speed - maxSpeed * 1.5f) / (maxSpeed * 3.0f - maxSpeed * 1.5f), 0.0f, 1.0f);
			const float vignetteRadius = 0.5f - 0.1f * effectProgress;
			// 加速中は爆発・トレイルと重なりやすいため、軽量なビネットだけを使う。
			PostEffect::GetInstance()->SetVignette(vignetteRadius, 0.4f);
		}
	}

	if (player) {
		damageFeedbackState.ObservePlayerHP(player->GetHP());
	}

	if (damageFeedbackState.ConsumeEffectFrame()) {
		PostEffect::GetInstance()->SetEffectType(13); // Fold Wave
	} else if (!isBoosting) {
		PostEffect::GetInstance()->SetEffectType(0); // 0: Normal
	}
}

void PlayerFlightEffectController::EmitLowAltitudeSpeedRush(const PlayerFlightEffectContext& context) const {
	// ファイターで地表すれすれを高速飛行している時だけ、カメラの少し前から
	// 後方へ流れる細い大気・ダストを発生させる。
	if (!context.shouldUpdateGame || context.isGameOverActive || context.isTitleBackgroundMode ||
		!context.player || !context.activeCamera || !context.particleManager || !context.balanceSettings ||
		context.player->GetCurrentMode() != PlayerMode::Fighter) {
		return;
	}

	const GameplayBalanceSettings& rush = *context.balanceSettings;
	if (!rush.speedRushEnabled) {
		return;
	}

	const float fighterMaxSpeed = (std::max)(0.01f, context.player->GetModeParams(PlayerMode::Fighter).maxMoveSpeed);
	const float speedRatio = MyMath::Length(context.player->GetVelocity()) / fighterMaxSpeed;
	const float altitudeFactor = std::clamp(1.0f - context.player->GetPosition().y / rush.speedRushMaxAltitude, 0.0f, 1.0f);
	const float speedRange = (std::max)(0.05f, 1.20f - rush.speedRushMinSpeedRatio);
	const float speedFactor = std::clamp((speedRatio - rush.speedRushMinSpeedRatio) / speedRange, 0.0f, 1.0f);
	const float intensity = altitudeFactor * speedFactor;
	if (intensity <= 0.01f) {
		return;
	}

	const OBB playerObb = context.player->GetOBB();
	const Vector3 forward = NormalizeOrVector3(playerObb.orientations[2], { 0.0f, 0.0f, 1.0f });
	const Vector3 right = NormalizeOrVector3(playerObb.orientations[0], { 1.0f, 0.0f, 0.0f });
	const Vector3 up = NormalizeOrVector3(playerObb.orientations[1], { 0.0f, 1.0f, 0.0f });
	const Vector3 backward = { -forward.x, -forward.y, -forward.z };
	const int particleCount = (std::max)(1, static_cast<int>(std::ceil(
		static_cast<float>(rush.speedRushMaxParticlesPerFrame) * intensity)));
	const float frameSeed = static_cast<float>(context.gameplayElapsedFrames);
	for (int index = 0; index < particleCount; ++index) {
		const float seed = frameSeed * 1.731f + static_cast<float>(index) * 13.217f;
		const float side = std::sin(seed * 2.13f) * rush.speedRushSpawnHalfWidth;
		const float height = std::sin(seed * 5.71f + 0.8f) * rush.speedRushSpawnHalfHeight;
		const float depth = rush.speedRushSpawnDistance + std::sin(seed * 3.47f) * 1.8f;
		const Vector3 spawnPosition = AddVector3(context.activeCamera->GetTranslate(), AddVector3(
			ScaleVector3(forward, depth), AddVector3(ScaleVector3(right, side), ScaleVector3(up, height))));
		context.particleManager->EmitDirectionalBurst(
			kSpeedRushParticleGroup, spawnPosition, backward, 1,
			{ 0.86f, 0.78f, 0.62f, 0.38f + intensity * 0.42f }, 0.12f,
			rush.speedRushParticleSpeed * (0.75f + intensity * 0.55f), 0.12f,
			rush.speedRushParticleScale * (0.70f + intensity * 0.45f), rush.speedRushParticleScale * 0.32f,
			rush.speedRushParticleLifetime * 0.72f, rush.speedRushParticleLifetime * 1.18f, 0.18f);
	}
}

void PlayerFlightEffectController::UpdateAfterPlayerMovement(const PlayerFlightEffectContext& context) const {
	EmitLowAltitudeSpeedRush(context);

	// プレイヤーの移動・加速処理が完了した後の座標を履歴に入れる。
	// これにより、高速移動時にも残像リボンの先端が機体から離れない。
	if (context.songState && context.songState->IsActive() && context.shouldUpdateGame &&
		context.player && context.songFlightTrailRenderer) {
		context.songFlightTrailRenderer->Update(*context.player);
	}

	if (context.warpExitPresentation) {
		context.warpExitPresentation->UpdateGate(context.activeCamera);
	}
}
