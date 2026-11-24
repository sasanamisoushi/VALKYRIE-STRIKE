#pragma once

#include "Game/Player/Player.h"

#include <array>
#include <string>

// ゲームプレイ全体のバランス設定値。
struct GameplayBalanceSettings {
	std::array<PlayerModeParams, 3> playerModes{};
	float specialAttackCost = 50.0f;
	float spGaugeRecoveryPerSecond = 3.0f;
	int specialAttackDurationFrames = 180;
	int specialAttackFireIntervalFrames = 6;
	int songDurationFrames = 900;
	int bossMaxActiveMinions = 6;
	// ファイターの低空高速飛行時に、カメラ近傍を後方へ流すスピード線。
	bool speedRushEnabled = true;
	float speedRushMaxAltitude = 24.0f;
	float speedRushMinSpeedRatio = 0.65f;
	int speedRushMaxParticlesPerFrame = 4;
	float speedRushSpawnDistance = 8.0f;
	float speedRushSpawnHalfWidth = 5.5f;
	float speedRushSpawnHalfHeight = 2.8f;
	float speedRushParticleSpeed = 0.62f;
	float speedRushParticleScale = 0.18f;
	float speedRushParticleLifetime = 0.34f;
};

// バランス設定 JSON に同梱されるミサイル調整値の受け渡し構造体。
struct GameplayBalanceMissileSettings {
	float normalSpeed = 1.5f;
	float normalHomingStrength = 0.28f;
	float normalScale = 0.3f;
	float normalCollisionRadius = 0.3f;
	int normalLifeTime = 120;
	float homingSpeed = 0.75f;
	float homingStrength = 0.085f;
	float homingScale = 0.5f;
	float homingCollisionRadius = 0.5f;
	float homingTrailWidth = 0.5f;
	int homingLifeTime = 240;
	float muzzleOffset = 0.8f;
};

// ゲームバランス設定の値検証・反映・JSON 永続化をまとめて管理する。
class GameplayBalanceSettingsManager {
public:
	const GameplayBalanceSettings& Get() const { return settings_; }

	void CaptureDefaults(Player* player);
	void Set(GameplayBalanceSettings settings, Player* player, GameplayBalanceMissileSettings* missileSettings = nullptr);
	void Apply(Player* player, GameplayBalanceMissileSettings* missileSettings = nullptr);
	bool Load(const std::string& filePath, Player* player, GameplayBalanceMissileSettings* missileSettings = nullptr);
	bool Save(const std::string& filePath, const GameplayBalanceMissileSettings* missileSettings = nullptr) const;

private:
	static void ClampMissileSettings(GameplayBalanceMissileSettings& missileSettings);

	GameplayBalanceSettings settings_;
};
