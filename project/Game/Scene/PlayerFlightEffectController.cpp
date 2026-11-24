#include "PlayerFlightEffectController.h"

#include "DamageFeedbackState.h"
#include "GameplayBalanceSettingsManager.h"
#include "SongFlightTrailRenderer.h"
#include "SongState.h"
#include "WarpExitPresentationState.h"
#include "Game/Player/Player.h"
#include "Game/obstacle/Obstacle.h"
#include "engine/Camera/Camera.h"
#include "engine/Graphics/PostEffect.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/math/MyMath.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr char kSpeedRushParticleGroup[] = "lowAltitudeSpeedRush";
constexpr char kDownwashParticleGroup[] = "lowAltitudeDownwash";
constexpr char kDustTrailParticleGroup[] = "lowAltitudeDustTrail";
constexpr char kDustDebrisParticleGroup[] = "lowAltitudeDustDebris";
constexpr float kPi = 3.14159265f;

float RandomRange(float minVal, float maxVal) {
	static std::mt19937 engine(std::random_device{}());
	std::uniform_real_distribution<float> dist(minVal, maxVal);
	return dist(engine);
}

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

bool GetTriangleY(const Vector3& p0, const Vector3& p1, const Vector3& p2, float x, float z, float& outY) {
	const float denominator = (p1.z - p2.z) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.z - p2.z);
	if (std::abs(denominator) < 0.00001f) {
		return false;
	}
	const float w0 = ((p1.z - p2.z) * (x - p2.x) + (p2.x - p1.x) * (z - p2.z)) / denominator;
	const float w1 = ((p2.z - p0.z) * (x - p2.x) + (p0.x - p2.x) * (z - p2.z)) / denominator;
	const float w2 = 1.0f - w0 - w1;
	if (w0 < -0.01f || w1 < -0.01f || w2 < -0.01f) {
		return false;
	}
	outY = w0 * p0.y + w1 * p1.y + w2 * p2.y;
	return true;
}

bool SampleGroundBelow(const std::list<std::unique_ptr<Obstacle>>& obstacles, const Vector3& playerPosition,
	float& outGroundY) {
	Sphere probe{};
	probe.center = playerPosition;
	probe.radius = 1.5f;
	bool foundGround = false;
	float highestGround = -100000.0f;
	for (const std::unique_ptr<Obstacle>& obstacle : obstacles) {
		if (!obstacle || obstacle->IsStageBounds() || !obstacle->IsCollisionEnabled() || !obstacle->IsUseMeshCollider()) {
			continue;
		}
		const std::vector<Triangle>& triangles = obstacle->GetWorldTriangles();
		for (const size_t triangleIndex : obstacle->GetNearbyWorldTriangleIndices(probe)) {
			const Triangle& triangle = triangles[triangleIndex];
			if (std::abs(triangle.normal.y) < 0.15f) {
				continue;
			}
			float groundY = 0.0f;
			if (GetTriangleY(triangle.p[0], triangle.p[1], triangle.p[2], playerPosition.x, playerPosition.z, groundY) &&
				groundY <= playerPosition.y + 0.1f && groundY > highestGround) {
				highestGround = groundY;
				foundGround = true;
			}
		}
	}
	if (foundGround) {
		outGroundY = highestGround;
	}
	return foundGround;
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

void PlayerFlightEffectController::EmitLowAltitudeDownwash(const PlayerFlightEffectContext& context) const {
	if (!context.shouldUpdateGame || context.isGameOverActive || context.isTitleBackgroundMode ||
		!context.player || !context.particleManager || !context.obstacles || !context.balanceSettings) {
		return;
	}
	const GameplayBalanceSettings& dust = *context.balanceSettings;
	if (!dust.lowAltitudeDustEnabled) {
		return;
	}

	float groundY = 0.0f;
	const Vector3 playerPosition = context.player->GetPosition();
	if (!SampleGroundBelow(*context.obstacles, playerPosition, groundY)) {
		return;
	}
	const float groundDistance = playerPosition.y - groundY;
	if (groundDistance < 0.0f || groundDistance > dust.lowAltitudeDustMaxGroundDistance) {
		return;
	}

	const float playerSpeed = MyMath::Length(context.player->GetVelocity());
	const float maxSpeed = (std::max)(0.01f, context.player->GetModeParams(context.player->GetCurrentMode()).maxMoveSpeed);
	const float rawSpeedRatio = playerSpeed / maxSpeed;
	const float speedFactor = std::clamp(rawSpeedRatio, 0.0f, 1.0f);
	const float boostFactor = std::clamp(rawSpeedRatio, 0.0f, 2.5f);
	const float heightFactor = 1.0f - groundDistance / dust.lowAltitudeDustMaxGroundDistance;
	const float intensity = speedFactor * heightFactor;
	if (intensity < dust.lowAltitudeDustMinIntensity) {
		return;
	}

	const OBB playerObb = context.player->GetOBB();
	const Vector3 forward = NormalizeOrVector3(
		{ playerObb.orientations[2].x, 0.0f, playerObb.orientations[2].z }, { 0.0f, 0.0f, 1.0f });
	const Vector3 backDir = { -forward.x, 0.0f, -forward.z };
	const Vector3 rightDir = { forward.z, 0.0f, -forward.x };
	// 常に機体ではなく、真下に投影した地面を発生源にする。
	const Vector3 groundHitPos = { playerPosition.x, groundY + 0.10f, playerPosition.z };
	const bool isHighSpeedFlight =
		(context.player->GetCurrentMode() == PlayerMode::Fighter && rawSpeedRatio >= 0.40f) ||
		rawSpeedRatio >= 0.75f;

	if (!isHighSpeedFlight) {
		// ホバー／低速時：地面に張り付いたドーナツ状のダウンウォッシュ。
		if (context.gameplayElapsedFrames % 5 != 0) {
			return;
		}
		const Vector4 dustColor = { 0.56f, 0.45f, 0.29f,
			std::clamp((0.18f + intensity * 0.30f) * dust.lowAltitudeDustOpacityMultiplier, 0.0f, 1.0f) };
		constexpr int kRingDirectionCount = 8;
		constexpr float kTwoPi = 6.28318530f;
		for (int index = 0; index < kRingDirectionCount; ++index) {
			const float angle = kTwoPi * static_cast<float>(index) / static_cast<float>(kRingDirectionCount) +
				static_cast<float>(context.gameplayElapsedFrames) * 0.035f;
			const Vector3 outward = { std::cos(angle), 0.10f, std::sin(angle) };
			context.particleManager->EmitDirectionalBurst(kDownwashParticleGroup, groundHitPos, outward, 1,
				dustColor, 0.18f, 0.30f + intensity * 0.26f, 0.08f,
				(0.40f + intensity * 0.26f) * dust.lowAltitudeDustScaleMultiplier, 0.10f,
				0.45f * dust.lowAltitudeDustLifetimeMultiplier, 0.76f * dust.lowAltitudeDustLifetimeMultiplier, 0.22f);
		}
		return;
	}

	// =========================================================================
	// ポッドレース風「超高速・低空砂煙」の3層構造
	// =========================================================================

	// ① 【核心】超高速で「後方＋横（ハの字）」へ吹き飛ぶ激しい土埃（ジェット風圧）
	const int jetDustCount = std::clamp(
		static_cast<int>(std::round((1.5f + boostFactor * 2.2f) * (0.45f + heightFactor * 0.55f) * dust.lowAltitudeDustEmissionMultiplier)),
		1, 16);
	for (int i = 0; i < jetDustCount; ++i) {
		// 左右のハの字に勢いよく逃がす方向ベクトル
		const float sideSign = (i % 2 == 0) ? 1.0f : -1.0f;
		const float sideFactor = sideSign * RandomRange(0.55f, 1.65f);
		const Vector3 pushDir = NormalizeOrVector3(
			AddVector3(ScaleVector3(backDir, 2.0f), ScaleVector3(rightDir, sideFactor)),
			backDir);

		const float finalSpeed = (playerSpeed * 0.85f + RandomRange(0.24f, 0.54f)) * (0.65f + heightFactor * 0.45f);
		const float startScale = RandomRange(0.55f, 1.05f) * (0.65f + heightFactor * 0.50f) * dust.lowAltitudeDustScaleMultiplier;
		const float tone = RandomRange(0.90f, 1.12f);

		ParticleManager::CustomParticleParams params{};
		params.position = AddVector3(groundHitPos, AddVector3(
			ScaleVector3(rightDir, sideSign * RandomRange(0.4f, 1.4f)),
			ScaleVector3(backDir, RandomRange(0.0f, 0.9f))));
		params.velocity = {
			pushDir.x * finalSpeed,
			RandomRange(0.012f, 0.055f), // 上へは少しだけ跳ね上げる
			pushDir.z * finalSpeed,
		};
		params.color = {
			std::clamp(0.62f * tone, 0.0f, 1.0f),
			std::clamp(0.50f * tone, 0.0f, 1.0f),
			std::clamp(0.34f * tone, 0.0f, 1.0f),
			std::clamp((0.20f + intensity * 0.24f) * RandomRange(0.8f, 1.15f) * dust.lowAltitudeDustOpacityMultiplier, 0.08f, 0.48f),
		};
		params.initialScale = startScale;
		params.finalScale = startScale * RandomRange(2.4f, 3.8f);
		params.lifeTime = RandomRange(0.40f, 0.80f) * dust.lowAltitudeDustLifetimeMultiplier;
		params.velocityDrag = 0.045f;
		params.upwardAcceleration = 0.0007f;
		params.initialRotation = RandomRange(-kPi, kPi);
		params.angularVelocity = RandomRange(-0.08f, 0.08f);
		context.particleManager->EmitCustom(kDownwashParticleGroup, params);
	}

	// ② 後方にずっと残る、巨大で不規則な「モクモクした砂煙の帯（軌跡）」
	const int trailDustCount = std::clamp(
		static_cast<int>(std::round((1.2f + boostFactor * 1.4f) * (0.45f + heightFactor * 0.55f) * dust.lowAltitudeDustEmissionMultiplier)),
		1, 12);
	for (int i = 0; i < trailDustCount; ++i) {
		const float startScale = RandomRange(0.85f, 1.45f) * (0.70f + heightFactor * 0.40f) * dust.lowAltitudeDustScaleMultiplier;
		const float tone = RandomRange(0.88f, 1.10f);
		const float lateralOffset = RandomRange(-1.4f, 1.4f);

		ParticleManager::CustomParticleParams params{};
		params.position = AddVector3(groundHitPos, AddVector3(
			ScaleVector3(backDir, RandomRange(0.8f, 2.3f)),
			ScaleVector3(rightDir, lateralOffset)));
		params.position.y += RandomRange(0.05f, 0.35f);
		params.velocity = {
			backDir.x * (playerSpeed * 0.18f + RandomRange(0.02f, 0.07f)) + rightDir.x * RandomRange(-0.06f, 0.06f),
			RandomRange(0.015f, 0.042f), // ゆっくりフワフワ立ち上る
			backDir.z * (playerSpeed * 0.18f + RandomRange(0.02f, 0.07f)) + rightDir.z * RandomRange(-0.06f, 0.06f),
		};
		// 薄いアルファ（0.10〜0.20）で多数重ねて巨大なモヤの帯を作る
		params.color = {
			std::clamp(0.68f * tone, 0.0f, 1.0f),
			std::clamp(0.56f * tone, 0.0f, 1.0f),
			std::clamp(0.40f * tone, 0.0f, 1.0f),
			std::clamp(RandomRange(0.10f, 0.19f) * (0.60f + heightFactor * 0.40f) * dust.lowAltitudeDustOpacityMultiplier, 0.0f, 1.0f),
		};
		params.initialScale = startScale;
		// 1.5〜3.0秒かけて元の10倍前後まで急膨張させる
		params.finalScale = startScale * RandomRange(8.5f, 12.0f);
		params.lifeTime = RandomRange(1.50f, 3.00f) * dust.lowAltitudeDustLifetimeMultiplier;
		params.velocityDrag = 0.032f;
		params.upwardAcceleration = 0.00045f;
		params.initialRotation = RandomRange(-kPi, kPi);
		params.angularVelocity = RandomRange(-0.024f, 0.024f);
		context.particleManager->EmitCustom(kDustTrailParticleGroup, params);
	}

	// ③ 飛び散る「目に見える砂礫（デブリ）・小石」
	const int debrisCount = std::clamp(
		static_cast<int>(std::round((1.5f + boostFactor * 2.0f) * (0.45f + heightFactor * 0.55f) * dust.lowAltitudeDustEmissionMultiplier)),
		1, 16);
	for (int i = 0; i < debrisCount; ++i) {
		const float sideFactor = RandomRange(-1.85f, 1.85f);
		const Vector3 scatterDir = NormalizeOrVector3(
			AddVector3(ScaleVector3(backDir, RandomRange(0.9f, 1.9f)), ScaleVector3(rightDir, sideFactor)),
			backDir);
		const float horizontalSpeed = (playerSpeed * 0.90f + RandomRange(0.18f, 0.48f)) * (0.65f + heightFactor * 0.45f);
		const float pebbleScale = RandomRange(0.055f, 0.145f) * dust.lowAltitudeDustScaleMultiplier;
		const float tone = RandomRange(0.75f, 1.15f);

		ParticleManager::CustomParticleParams params{};
		params.position = AddVector3(groundHitPos, AddVector3(
			ScaleVector3(rightDir, RandomRange(-1.3f, 1.3f)),
			ScaleVector3(backDir, RandomRange(-0.2f, 0.8f))));
		params.velocity = {
			scatterDir.x * horizontalSpeed,
			RandomRange(0.10f, 0.28f), // 勢いよく跳ね上げて放物線を描く
			scatterDir.z * horizontalSpeed,
		};
		params.color = {
			std::clamp(0.42f * tone, 0.0f, 1.0f),
			std::clamp(0.34f * tone, 0.0f, 1.0f),
			std::clamp(0.23f * tone, 0.0f, 1.0f),
			std::clamp(RandomRange(0.78f, 0.95f) * dust.lowAltitudeDustOpacityMultiplier, 0.0f, 1.0f),
		};
		params.initialScale = pebbleScale;
		params.finalScale = pebbleScale * 0.65f;
		params.lifeTime = RandomRange(0.45f, 0.95f) * dust.lowAltitudeDustLifetimeMultiplier;
		params.velocityDrag = 0.010f;
		// 強めの重力で放物線を描いて落下させる
		params.upwardAcceleration = -0.014f;
		params.initialRotation = RandomRange(-kPi, kPi);
		params.angularVelocity = RandomRange(-0.28f, 0.28f);
		context.particleManager->EmitCustom(kDustDebrisParticleGroup, params);
	}
}

void PlayerFlightEffectController::UpdateAfterPlayerMovement(const PlayerFlightEffectContext& context) const {
	EmitLowAltitudeSpeedRush(context);
	EmitLowAltitudeDownwash(context);

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
