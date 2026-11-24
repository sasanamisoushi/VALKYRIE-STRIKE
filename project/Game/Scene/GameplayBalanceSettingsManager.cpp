#include "GameplayBalanceSettingsManager.h"

#include "externals/json.hpp"

#include <algorithm>
#include <exception>
#include <fstream>

using json = nlohmann::json;

void GameplayBalanceSettingsManager::CaptureDefaults(Player* player) {
	if (!player) {
		return;
	}
	for (int mode = 0; mode < 3; ++mode) {
		settings_.playerModes[mode] = player->GetModeParams(static_cast<PlayerMode>(mode));
	}
}

void GameplayBalanceSettingsManager::Set(
	GameplayBalanceSettings settings,
	Player* player,
	GameplayBalanceMissileSettings* missileSettings) {
	settings_ = settings;
	Apply(player, missileSettings);
}

void GameplayBalanceSettingsManager::ClampMissileSettings(GameplayBalanceMissileSettings& missileSettings) {
	missileSettings.normalSpeed = std::clamp(missileSettings.normalSpeed, 0.01f, 10.0f);
	missileSettings.normalHomingStrength = std::clamp(missileSettings.normalHomingStrength, 0.0f, 1.0f);
	missileSettings.normalScale = std::clamp(missileSettings.normalScale, 0.01f, 5.0f);
	missileSettings.normalCollisionRadius = std::clamp(missileSettings.normalCollisionRadius, 0.01f, 10.0f);
	missileSettings.normalLifeTime = std::clamp(missileSettings.normalLifeTime, 1, 3600);
	missileSettings.homingSpeed = std::clamp(missileSettings.homingSpeed, 0.01f, 10.0f);
	missileSettings.homingStrength = std::clamp(missileSettings.homingStrength, 0.0f, 1.0f);
	missileSettings.homingScale = std::clamp(missileSettings.homingScale, 0.01f, 5.0f);
	missileSettings.homingCollisionRadius = std::clamp(missileSettings.homingCollisionRadius, 0.01f, 10.0f);
	missileSettings.homingTrailWidth = std::clamp(missileSettings.homingTrailWidth, 0.0f, 10.0f);
	missileSettings.homingLifeTime = std::clamp(missileSettings.homingLifeTime, 1, 3600);
	missileSettings.muzzleOffset = std::clamp(missileSettings.muzzleOffset, 0.0f, 20.0f);
}

void GameplayBalanceSettingsManager::Apply(
	Player* player,
	GameplayBalanceMissileSettings* missileSettings) {
	settings_.specialAttackCost = std::clamp(settings_.specialAttackCost, 0.0f, 100.0f);
	settings_.spGaugeRecoveryPerSecond = std::clamp(settings_.spGaugeRecoveryPerSecond, 0.0f, 60.0f);
	settings_.specialAttackDurationFrames = std::clamp(settings_.specialAttackDurationFrames, 1, 3600);
	settings_.specialAttackFireIntervalFrames = std::clamp(settings_.specialAttackFireIntervalFrames, 1, 300);
	settings_.songDurationFrames = std::clamp(settings_.songDurationFrames, 60, 7200);
	settings_.bossMaxActiveMinions = std::clamp(settings_.bossMaxActiveMinions, 0, 24);
	settings_.speedRushMaxAltitude = std::clamp(settings_.speedRushMaxAltitude, 1.0f, 200.0f);
	settings_.speedRushMinSpeedRatio = std::clamp(settings_.speedRushMinSpeedRatio, 0.05f, 2.0f);
	settings_.speedRushMaxParticlesPerFrame = std::clamp(settings_.speedRushMaxParticlesPerFrame, 1, 16);
	settings_.speedRushSpawnDistance = std::clamp(settings_.speedRushSpawnDistance, 1.0f, 50.0f);
	settings_.speedRushSpawnHalfWidth = std::clamp(settings_.speedRushSpawnHalfWidth, 0.1f, 30.0f);
	settings_.speedRushSpawnHalfHeight = std::clamp(settings_.speedRushSpawnHalfHeight, 0.1f, 20.0f);
	settings_.speedRushParticleSpeed = std::clamp(settings_.speedRushParticleSpeed, 0.01f, 3.0f);
	settings_.speedRushParticleScale = std::clamp(settings_.speedRushParticleScale, 0.01f, 3.0f);
	settings_.speedRushParticleLifetime = std::clamp(settings_.speedRushParticleLifetime, 0.05f, 2.0f);

	if (missileSettings) {
		ClampMissileSettings(*missileSettings);
	}
	if (player) {
		for (int mode = 0; mode < 3; ++mode) {
			player->SetModeParams(static_cast<PlayerMode>(mode), settings_.playerModes[mode]);
			settings_.playerModes[mode] = player->GetModeParams(static_cast<PlayerMode>(mode));
		}
	}
}

bool GameplayBalanceSettingsManager::Load(
	const std::string& filePath,
	Player* player,
	GameplayBalanceMissileSettings* missileSettings) {
	std::ifstream input(filePath);
	if (!input.is_open()) {
		return false;
	}

	try {
		json root;
		input >> root;
		const json& source = root.contains("gameplay_balance") ? root["gameplay_balance"] : root;
		GameplayBalanceSettings loaded = settings_;
		auto readFloat = [&source](const char* name, float& value) {
			if (source.contains(name) && source[name].is_number()) {
				value = source[name].get<float>();
			}
		};
		auto readInt = [&source](const char* name, int& value) {
			if (source.contains(name) && source[name].is_number_integer()) {
				value = source[name].get<int>();
			}
		};

		readFloat("special_attack_cost", loaded.specialAttackCost);
		readFloat("sp_recovery_per_second", loaded.spGaugeRecoveryPerSecond);
		readInt("special_attack_duration_frames", loaded.specialAttackDurationFrames);
		readInt("special_attack_fire_interval_frames", loaded.specialAttackFireIntervalFrames);
		readInt("song_duration_frames", loaded.songDurationFrames);
		readInt("boss_max_active_minions", loaded.bossMaxActiveMinions);
		if (source.contains("speed_rush_enabled") && source["speed_rush_enabled"].is_boolean()) {
			loaded.speedRushEnabled = source["speed_rush_enabled"].get<bool>();
		}
		readFloat("speed_rush_max_altitude", loaded.speedRushMaxAltitude);
		readFloat("speed_rush_min_speed_ratio", loaded.speedRushMinSpeedRatio);
		readInt("speed_rush_max_particles_per_frame", loaded.speedRushMaxParticlesPerFrame);
		readFloat("speed_rush_spawn_distance", loaded.speedRushSpawnDistance);
		readFloat("speed_rush_spawn_half_width", loaded.speedRushSpawnHalfWidth);
		readFloat("speed_rush_spawn_half_height", loaded.speedRushSpawnHalfHeight);
		readFloat("speed_rush_particle_speed", loaded.speedRushParticleSpeed);
		readFloat("speed_rush_particle_scale", loaded.speedRushParticleScale);
		readFloat("speed_rush_particle_lifetime", loaded.speedRushParticleLifetime);

		if (missileSettings) {
			readFloat("normal_speed", missileSettings->normalSpeed);
			readFloat("normal_homing_strength", missileSettings->normalHomingStrength);
			readFloat("normal_scale", missileSettings->normalScale);
			readFloat("normal_collision_radius", missileSettings->normalCollisionRadius);
			readInt("normal_lifetime", missileSettings->normalLifeTime);
			readFloat("homing_speed", missileSettings->homingSpeed);
			readFloat("homing_strength", missileSettings->homingStrength);
			readFloat("homing_scale", missileSettings->homingScale);
			readFloat("homing_collision_radius", missileSettings->homingCollisionRadius);
			readFloat("homing_trail_width", missileSettings->homingTrailWidth);
			readInt("homing_lifetime", missileSettings->homingLifeTime);
			readFloat("muzzle_offset", missileSettings->muzzleOffset);
		}

		if (source.contains("player_modes") && source["player_modes"].is_array()) {
			for (size_t mode = 0; mode < 3 && mode < source["player_modes"].size(); ++mode) {
				const json& p = source["player_modes"][mode];
				if (!p.is_object()) {
					continue;
				}
				auto& target = loaded.playerModes[mode];
				auto getFloat = [&p](const char* name, float& value) {
					if (p.contains(name) && p[name].is_number()) {
						value = p[name].get<float>();
					}
				};
				getFloat("max_speed", target.maxMoveSpeed);
				getFloat("acceleration", target.moveAcceleration);
				getFloat("damping", target.moveDamping);
				getFloat("pitch_speed", target.pitchSpeed);
				getFloat("yaw_speed", target.yawSpeed);
				getFloat("roll_speed", target.rollSpeed);
				getFloat("lock_angle_dot", target.lockOnAngleDot);
				getFloat("lock_distance", target.maxLockOnDistance);
				if (p.contains("max_multi_lock") && p["max_multi_lock"].is_number_integer()) {
					target.maxMultiLock = p["max_multi_lock"].get<int>();
				}
				if (p.contains("can_guard") && p["can_guard"].is_boolean()) {
					target.canGuard = p["can_guard"].get<bool>();
				}
				if (p.contains("can_melee") && p["can_melee"].is_boolean()) {
					target.canMelee = p["can_melee"].get<bool>();
				}
			}
		}

		Set(loaded, player, missileSettings);
		return true;
	} catch (const std::exception&) {
		return false;
	}
}

bool GameplayBalanceSettingsManager::Save(
	const std::string& filePath,
	const GameplayBalanceMissileSettings* missileSettings) const {
	std::ofstream output(filePath, std::ios::trunc);
	if (!output.is_open()) {
		return false;
	}

	const GameplayBalanceMissileSettings missiles = missileSettings ? *missileSettings : GameplayBalanceMissileSettings{};
	json modes = json::array();
	for (const PlayerModeParams& p : settings_.playerModes) {
		modes.push_back({
			{ "max_speed", p.maxMoveSpeed },
			{ "acceleration", p.moveAcceleration },
			{ "damping", p.moveDamping },
			{ "pitch_speed", p.pitchSpeed },
			{ "yaw_speed", p.yawSpeed },
			{ "roll_speed", p.rollSpeed },
			{ "can_guard", p.canGuard },
			{ "can_melee", p.canMelee },
			{ "max_multi_lock", p.maxMultiLock },
			{ "lock_angle_dot", p.lockOnAngleDot },
			{ "lock_distance", p.maxLockOnDistance },
		});
	}

	const json root = {
		{ "gameplay_balance", {
			{ "special_attack_cost", settings_.specialAttackCost },
			{ "sp_recovery_per_second", settings_.spGaugeRecoveryPerSecond },
			{ "special_attack_duration_frames", settings_.specialAttackDurationFrames },
			{ "special_attack_fire_interval_frames", settings_.specialAttackFireIntervalFrames },
			{ "song_duration_frames", settings_.songDurationFrames },
			{ "boss_max_active_minions", settings_.bossMaxActiveMinions },
			{ "speed_rush_enabled", settings_.speedRushEnabled },
			{ "speed_rush_max_altitude", settings_.speedRushMaxAltitude },
			{ "speed_rush_min_speed_ratio", settings_.speedRushMinSpeedRatio },
			{ "speed_rush_max_particles_per_frame", settings_.speedRushMaxParticlesPerFrame },
			{ "speed_rush_spawn_distance", settings_.speedRushSpawnDistance },
			{ "speed_rush_spawn_half_width", settings_.speedRushSpawnHalfWidth },
			{ "speed_rush_spawn_half_height", settings_.speedRushSpawnHalfHeight },
			{ "speed_rush_particle_speed", settings_.speedRushParticleSpeed },
			{ "speed_rush_particle_scale", settings_.speedRushParticleScale },
			{ "speed_rush_particle_lifetime", settings_.speedRushParticleLifetime },
			{ "normal_speed", missiles.normalSpeed },
			{ "normal_homing_strength", missiles.normalHomingStrength },
			{ "normal_scale", missiles.normalScale },
			{ "normal_collision_radius", missiles.normalCollisionRadius },
			{ "normal_lifetime", missiles.normalLifeTime },
			{ "homing_speed", missiles.homingSpeed },
			{ "homing_strength", missiles.homingStrength },
			{ "homing_scale", missiles.homingScale },
			{ "homing_collision_radius", missiles.homingCollisionRadius },
			{ "homing_trail_width", missiles.homingTrailWidth },
			{ "homing_lifetime", missiles.homingLifeTime },
			{ "muzzle_offset", missiles.muzzleOffset },
			{ "player_modes", modes },
		} }
	};
	output << root.dump(2) << '\n';
	return output.good();
}
