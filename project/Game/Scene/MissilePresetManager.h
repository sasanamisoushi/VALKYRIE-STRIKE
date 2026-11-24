#pragma once

#include <functional>
#include <string>
#include <vector>
#include "engine/Camera/Camera.h"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/bullet/Missile.h"
#include "externals/json.hpp"

class AmmoManager;
class MissileManager;

struct MissilePresetSceneContext {
	Player* player = nullptr;
	AmmoManager* ammoManager = nullptr;
	MissileManager* missileManager = nullptr;
	float* missileNormalSpeed = nullptr;
	float* missileNormalScale = nullptr;
	float* missileNormalCollisionRadius = nullptr;
	int* missileNormalLifeTime = nullptr;
	float* missileSpeed = nullptr;
	float* missileHomingStrength = nullptr;
	float* missileHomingScale = nullptr;
	float* missileHomingCollisionRadius = nullptr;
	float* missileTrailWidth = nullptr;
	int* missileLifeTime = nullptr;
	float* missileMuzzleOffset = nullptr;
	std::function<MissileTuning(MissileType)> makeMissileTuning;
	std::function<bool(MissileType)> tryConsumeAmmo;
	std::function<void()> drawAmmoSettingsUI;
};

class MissilePresetManager {
public:
	explicit MissilePresetManager(MissilePresetSceneContext context);
	void RefreshMissilePresetNames();
	bool SaveMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName);
	bool ApplyMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName);
	void DrawMissileSettingsUI();
	bool FirePlayerMissile(MissileType type, Enemy *target = nullptr, float horizontalOffset = 0.0f,
		bool consumeAmmo = true, const Vector3* targetPoint = nullptr);

private:
	MissilePresetSceneContext context_;
	char missilePresetName_[64] = "MissilePreset1";
	int missilePresetTypeIndex_ = 0;
	std::vector<std::string> missilePresetNames_[2];
	int selectedMissilePresetIndex_[2] = { 0, 0 };
	std::string missilePresetMessage_;
};
