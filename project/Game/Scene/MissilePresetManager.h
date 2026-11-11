#pragma once

#include <string>
#include <vector>
#include "engine/Camera/Camera.h"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/bullet/Missile.h"
#include "externals/json.hpp"

class GamePlayScene;

class MissilePresetManager {
public:
	MissilePresetManager(GamePlayScene* scene);
	void RefreshMissilePresetNames();
	bool SaveMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName);
	bool ApplyMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName);
	void DrawMissileSettingsUI();
	bool FirePlayerMissile(MissileType type, Enemy *target = nullptr, float horizontalOffset = 0.0f,
		bool consumeAmmo = true, const Vector3* targetPoint = nullptr);

private:
	GamePlayScene* scene_;
};
