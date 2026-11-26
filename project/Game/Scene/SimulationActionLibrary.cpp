#include "SimulationActionLibrary.h"

#include "EnemyFactory.h"
#include "EnemySpawnManager.h"
#include "Game/Player/Player.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"
#include "GameOverSequence.h"
#include "GamePlaySceneHelpers.h"
#include "LockOnManager.h"
#include "TargetSelectionState.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Particle/ExplosionManager.h"
#include "engine/Utility/StageLoader.h"

#include <Windows.h>
#include <algorithm>
#include <exception>
#include <fstream>

void SimulationActionLibrary::RefreshActionNames(
	const std::string& filePath, std::vector<std::string>& actionNames, int& selectedIndex) {
	actionNames.clear();

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		selectedIndex = 0;
		return;
	}

	json root;
	try {
		ifs >> root;
	} catch (const std::exception& e) {
		OutputDebugStringA((std::string("[SimulationActionLibrary] Failed to read actions JSON: ") + e.what() + "\n").c_str());
		selectedIndex = 0;
		return;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		selectedIndex = 0;
		return;
	}

	for (const auto& actionData : root["actions"]) {
		if (!actionData.is_object() || !actionData.contains("name") || !actionData["name"].is_string()) {
			continue;
		}
		actionNames.push_back(actionData["name"].get<std::string>());
	}

	if (selectedIndex >= static_cast<int>(actionNames.size())) {
		selectedIndex = actionNames.empty() ? 0 : static_cast<int>(actionNames.size()) - 1;
	}
}

bool SimulationActionLibrary::SaveNamedAction(
	const std::string& filePath, const std::string& actionName, const SimulationActionContext& context) {
	auto setStatus = [&context](const std::string& msg) {
		if (context.statusMessage) {
			*context.statusMessage = msg;
		}
	};

	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		setStatus("保存名を入力してください。");
		return false;
	}

	json root;
	{
		std::ifstream ifs(filePath);
		if (ifs.is_open()) {
			try {
				ifs >> root;
			} catch (const std::exception& e) {
				OutputDebugStringA((std::string("[SimulationActionLibrary] Resetting unreadable actions JSON: ") + e.what() + "\n").c_str());
				root = json::object();
			}
		}
	}

	if (!root.is_object()) {
		root = json::object();
	}
	if (!root.contains("actions") || !root["actions"].is_array()) {
		root["actions"] = json::array();
	}

	json action;
	action["name"] = trimmedName;

	if (context.player) {
		json playerData;
		playerData["position"] = ToVector3Json(context.player->GetPosition());
		playerData["rotation"] = ToVector3Json(EulerFromQuaternionXYZ(context.player->GetQuaternion()));
		playerData["mode"] = static_cast<int>(context.player->GetCurrentMode());

		json modeParams = json::array();
		for (int modeIndex = 0; modeIndex < 3; ++modeIndex) {
			modeParams.push_back(PlayerModeParamsToJson(context.player->GetModeParams(static_cast<PlayerMode>(modeIndex))));
		}
		playerData["modeParams"] = modeParams;
		action["player"] = playerData;
	}

	const auto& mp = context.missileParams;
	json missileData;
	if (mp.normalSpeed) missileData["normalSpeed"] = *mp.normalSpeed;
	if (mp.normalScale) missileData["normalScale"] = *mp.normalScale;
	if (mp.normalCollisionRadius) missileData["normalCollisionRadius"] = *mp.normalCollisionRadius;
	if (mp.normalLifeTime) missileData["normalLifeTime"] = *mp.normalLifeTime;
	if (mp.speed) missileData["speed"] = *mp.speed;
	if (mp.ampX) missileData["ampX"] = *mp.ampX;
	if (mp.ampZ) missileData["ampZ"] = *mp.ampZ;
	if (mp.ampY) missileData["ampY"] = *mp.ampY;
	if (mp.freqY) missileData["freqY"] = *mp.freqY;
	if (mp.baseY) missileData["baseY"] = *mp.baseY;
	if (mp.homingStrength) missileData["homingStrength"] = *mp.homingStrength;
	if (mp.homingScale) missileData["homingScale"] = *mp.homingScale;
	if (mp.homingCollisionRadius) missileData["homingCollisionRadius"] = *mp.homingCollisionRadius;
	if (mp.trailWidth) missileData["trailWidth"] = *mp.trailWidth;
	if (mp.lifeTime) missileData["lifeTime"] = *mp.lifeTime;
	if (mp.muzzleOffset) missileData["muzzleOffset"] = *mp.muzzleOffset;
	action["missile"] = missileData;

	json enemiesData = json::array();
	if (context.enemies) {
		int generatedEnemyIndex = 1;
		for (const auto& enemy : *context.enemies) {
			if (!enemy || enemy->IsDead()) {
				continue;
			}

			json enemyData;
			const size_t spawnPointIndex = enemy->GetSpawnPointIndex();
			if (context.enemySpawns && spawnPointIndex < context.enemySpawns->size()) {
				const EnemySpawnData& spawnData = (*context.enemySpawns)[spawnPointIndex];
				enemyData["name"] = spawnData.name;
				enemyData["reinforcementTrigger"] = spawnData.reinforcementTriggerName;
				enemyData["reinforcementDelay"] = spawnData.reinforcementDelayFrames;
			} else {
				enemyData["name"] = "SimEnemy" + std::to_string(generatedEnemyIndex++);
			}

			enemyData["position"] = ToVector3Json(enemy->GetPosition());
			enemyData["rotation"] = ToVector3Json(enemy->GetRotation());
			enemyData["scale"] = ToVector3Json(enemy->GetScale());
			enemyData["initial"] = true;
			enemiesData.push_back(enemyData);
		}
	}
	action["enemies"] = enemiesData;

	json obstaclesData = json::array();
	if (context.obstacles) {
		for (const auto& obstacle : *context.obstacles) {
			if (!obstacle) {
				continue;
			}

			json obstacleData;
			obstacleData["position"] = ToVector3Json(obstacle->GetPosition());
			obstacleData["rotation"] = ToVector3Json(obstacle->GetRotation());
			obstacleData["scale"] = ToVector3Json(obstacle->GetScale());
			obstacleData["collisionOffset"] = ToVector3Json(obstacle->GetCollisionOffset());
			obstacleData["collisionScale"] = ToVector3Json(obstacle->GetCollisionScale());
			obstacleData["isCollisionEnabled"] = obstacle->IsCollisionEnabled();
			obstacleData["useMeshCollider"] = obstacle->IsUseMeshCollider();
			obstacleData["stageBounds"] = obstacle->IsStageBounds();
			obstaclesData.push_back(obstacleData);
		}
	}
	action["obstacles"] = obstaclesData;

	if (context.explosionManager) {
		const auto& config = context.explosionManager->GetConfig();
		json explosionData;
		explosionData["count"] = config.count;
		explosionData["color"] = json::array({ config.color[0], config.color[1], config.color[2], config.color[3] });
		explosionData["speed"] = config.speed;
		explosionData["speedVariance"] = config.speedVariance;
		explosionData["scale"] = config.scale;
		explosionData["scaleVariance"] = config.scaleVariance;
		explosionData["lifeTimeMin"] = config.lifeTimeMin;
		explosionData["lifeTimeMax"] = config.lifeTimeMax;
		explosionData["posVariance"] = config.posVariance;
		explosionData["destructionParticleType"] = config.destructionParticleType;
		explosionData["smokeColor"] = json::array({ config.smokeColor[0], config.smokeColor[1], config.smokeColor[2], config.smokeColor[3] });
		explosionData["flameToSmokeScale"] = config.flameToSmokeScale;
		explosionData["flameToSmokeTransition"] = config.flameToSmokeTransition;
		action["explosion"] = explosionData;
	}

	json& actions = root["actions"];
	bool updated = false;
	for (auto& existingAction : actions) {
		if (existingAction.is_object() && existingAction.value("name", "") == trimmedName) {
			existingAction = action;
			updated = true;
			break;
		}
	}
	if (!updated) {
		actions.push_back(action);
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		setStatus("名前付き行動を保存できませんでした。");
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	if (context.actionNames && context.selectedActionIndex) {
		RefreshActionNames(filePath, *context.actionNames, *context.selectedActionIndex);
		for (size_t index = 0; index < context.actionNames->size(); ++index) {
			if ((*context.actionNames)[index] == trimmedName) {
				*context.selectedActionIndex = static_cast<int>(index);
				break;
			}
		}
	}

	setStatus("行動「" + trimmedName + "」を保存しました。");
	return true;
}

bool SimulationActionLibrary::ApplyAction(
	const std::string& filePath, const std::string& actionName, const SimulationActionContext& context) {
	auto setStatus = [&context](const std::string& msg) {
		if (context.statusMessage) {
			*context.statusMessage = msg;
		}
	};

	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		setStatus("読み込む行動名がありません。");
		return false;
	}

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		setStatus("名前付き行動ファイルが見つかりません。");
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (const std::exception& e) {
		OutputDebugStringA((std::string("[SimulationActionLibrary] Failed to apply action JSON: ") + e.what() + "\n").c_str());
		setStatus("名前付き行動ファイルを読み込めませんでした。");
		return false;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		setStatus("保存された行動がありません。");
		return false;
	}

	const json* action = nullptr;
	for (const auto& actionData : root["actions"]) {
		if (actionData.is_object() && actionData.value("name", "") == trimmedName) {
			action = &actionData;
			break;
		}
	}

	if (!action) {
		setStatus("選択した行動が見つかりません。");
		return false;
	}

	if (context.targetSelection) context.targetSelection->ClearAll();
	if (context.lockOnManager) context.lockOnManager->CancelMultiLock();
	if (context.resetCinematicLockOnCamera) context.resetCinematicLockOnCamera();
	if (context.gameOverSequence) context.gameOverSequence->Reset();

	if (context.player && action->contains("player") && (*action)["player"].is_object()) {
		const json& playerData = (*action)["player"];
		if (playerData.contains("modeParams") && playerData["modeParams"].is_array()) {
			const json& modeParams = playerData["modeParams"];
			for (int modeIndex = 0; modeIndex < 3 && modeIndex < static_cast<int>(modeParams.size()); ++modeIndex) {
				PlayerModeParams params = context.player->GetModeParams(static_cast<PlayerMode>(modeIndex));
				ApplyPlayerModeParamsFromJson(modeParams[modeIndex], params);
				context.player->SetModeParams(static_cast<PlayerMode>(modeIndex), params);
			}
		}

		const int modeIndex = std::clamp(ReadJsonInt(playerData, "mode", 0), 0, 2);
		context.player->ChangeMode(static_cast<PlayerMode>(modeIndex));
		context.player->SetPosition(ReadVector3Json(playerData.value("position", json::array()), context.player->GetPosition()));
		context.player->SetRotation(ReadVector3Json(playerData.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f }));
	}

	if (action->contains("missile") && (*action)["missile"].is_object()) {
		const json& missileData = (*action)["missile"];
		const auto& mp = context.missileParams;
		if (mp.normalSpeed) *mp.normalSpeed = ReadJsonFloat(missileData, "normalSpeed", *mp.normalSpeed);
		if (mp.normalScale) *mp.normalScale = ReadJsonFloat(missileData, "normalScale", *mp.normalScale);
		if (mp.normalCollisionRadius) *mp.normalCollisionRadius = ReadJsonFloat(missileData, "normalCollisionRadius", *mp.normalCollisionRadius);
		if (mp.normalLifeTime) *mp.normalLifeTime = ReadJsonInt(missileData, "normalLifeTime", *mp.normalLifeTime);
		if (mp.speed) *mp.speed = ReadJsonFloat(missileData, "speed", *mp.speed);
		if (mp.ampX) *mp.ampX = ReadJsonFloat(missileData, "ampX", *mp.ampX);
		if (mp.ampZ) *mp.ampZ = ReadJsonFloat(missileData, "ampZ", *mp.ampZ);
		if (mp.ampY) *mp.ampY = ReadJsonFloat(missileData, "ampY", *mp.ampY);
		if (mp.freqY) *mp.freqY = ReadJsonFloat(missileData, "freqY", *mp.freqY);
		if (mp.baseY) *mp.baseY = ReadJsonFloat(missileData, "baseY", *mp.baseY);
		if (mp.homingStrength) *mp.homingStrength = ReadJsonFloat(missileData, "homingStrength", *mp.homingStrength);
		if (mp.homingScale) *mp.homingScale = ReadJsonFloat(missileData, "homingScale", *mp.homingScale);
		if (mp.homingCollisionRadius) *mp.homingCollisionRadius = ReadJsonFloat(missileData, "homingCollisionRadius", *mp.homingCollisionRadius);
		if (mp.trailWidth) *mp.trailWidth = ReadJsonFloat(missileData, "trailWidth", *mp.trailWidth);
		if (mp.lifeTime) *mp.lifeTime = ReadJsonInt(missileData, "lifeTime", *mp.lifeTime);
		if (mp.muzzleOffset) *mp.muzzleOffset = ReadJsonFloat(missileData, "muzzleOffset", *mp.muzzleOffset);
	}

	if (context.explosionManager && action->contains("explosion") && (*action)["explosion"].is_object()) {
		const json& explosionData = (*action)["explosion"];
		auto& config = context.explosionManager->GetConfig();
		config.count = ReadJsonInt(explosionData, "count", config.count);
		if (explosionData.contains("color") && explosionData["color"].is_array() && explosionData["color"].size() >= 4) {
			for (int index = 0; index < 4; ++index) {
				config.color[index] = explosionData["color"][index].get<float>();
			}
		}
		config.speed = ReadJsonFloat(explosionData, "speed", config.speed);
		config.speedVariance = ReadJsonFloat(explosionData, "speedVariance", config.speedVariance);
		config.scale = ReadJsonFloat(explosionData, "scale", config.scale);
		config.scaleVariance = ReadJsonFloat(explosionData, "scaleVariance", config.scaleVariance);
		config.lifeTimeMin = ReadJsonFloat(explosionData, "lifeTimeMin", config.lifeTimeMin);
		config.lifeTimeMax = ReadJsonFloat(explosionData, "lifeTimeMax", config.lifeTimeMax);
		config.posVariance = ReadJsonFloat(explosionData, "posVariance", config.posVariance);
		config.destructionParticleType = (std::clamp)(ReadJsonInt(explosionData, "destructionParticleType", config.destructionParticleType), 0, 3);
		if (explosionData.contains("smokeColor") && explosionData["smokeColor"].is_array() && explosionData["smokeColor"].size() >= 4) {
			for (int index = 0; index < 4; ++index) {
				config.smokeColor[index] = explosionData["smokeColor"][index].get<float>();
			}
		}
		config.flameToSmokeScale = ReadJsonFloat(explosionData, "flameToSmokeScale", config.flameToSmokeScale);
		config.flameToSmokeTransition = ReadJsonFloat(explosionData, "flameToSmokeTransition", config.flameToSmokeTransition);
	}

	if (context.obstacles && action->contains("obstacles") && (*action)["obstacles"].is_array()) {
		auto obstacleIt = context.obstacles->begin();
		for (const auto& obstacleData : (*action)["obstacles"]) {
			if (obstacleIt == context.obstacles->end()) {
				break;
			}
			if (!(*obstacleIt)) {
				++obstacleIt;
				continue;
			}

			(*obstacleIt)->SetPosition(ReadVector3Json(obstacleData.value("position", json::array()), (*obstacleIt)->GetPosition()));
			(*obstacleIt)->SetRotation(ReadVector3Json(obstacleData.value("rotation", json::array()), (*obstacleIt)->GetRotation()));
			(*obstacleIt)->SetScale(ReadVector3Json(obstacleData.value("scale", json::array()), (*obstacleIt)->GetScale()));
			(*obstacleIt)->SetCollisionOffset(ReadVector3Json(obstacleData.value("collisionOffset", json::array()), (*obstacleIt)->GetCollisionOffset()));
			(*obstacleIt)->SetCollisionScale(ReadVector3Json(obstacleData.value("collisionScale", json::array()), (*obstacleIt)->GetCollisionScale()));
			(*obstacleIt)->SetCollisionEnabled(obstacleData.value("isCollisionEnabled", (*obstacleIt)->IsCollisionEnabled()));
			(*obstacleIt)->SetUseMeshCollider(obstacleData.value("useMeshCollider", (*obstacleIt)->IsUseMeshCollider()));
			(*obstacleIt)->Update();
			++obstacleIt;
		}
	}

	if (context.enemies && context.enemySpawns && context.obstacles &&
		action->contains("enemies") && (*action)["enemies"].is_array()) {
		context.enemies->clear();
		context.enemySpawns->clear();

		for (const auto& enemyData : (*action)["enemies"]) {
			if (!enemyData.is_object()) {
				continue;
			}

			EnemySpawnData spawnData;
			spawnData.name = enemyData.value("name", "SavedEnemy");
			spawnData.position = ReadVector3Json(enemyData.value("position", json::array()), { 0.0f, 0.0f, 0.0f });
			spawnData.rotation = ReadVector3Json(enemyData.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f });
			spawnData.isInitialSpawn = enemyData.value("initial", true);
			spawnData.reinforcementTriggerName = enemyData.value("reinforcementTrigger", "");
			spawnData.reinforcementDelayFrames = ReadJsonInt(enemyData, "reinforcementDelay", 0);
			context.enemySpawns->push_back(spawnData);

			if (spawnData.isInitialSpawn) {
				EnemyFactory factory;
				if (auto enemy = factory.Create(spawnData, context.enemySpawns->size() - 1, *context.obstacles)) {
					enemy->SetScale(ReadVector3Json(enemyData.value("scale", json::array()), enemy->GetScale()));
					context.enemies->push_back(std::move(enemy));
				}
			}
		}
		if (context.enemySpawnManager) {
			context.enemySpawnManager->Reset(*context.enemySpawns);
		}
	}

	if (context.missileManager) {
		context.missileManager->Initialize(context.particleManager);
	}
	if (context.enemyBulletManager) {
		context.enemyBulletManager->Initialize();
	}

	if (!context.isDebugCameraActive && context.camera && context.player) {
		Vector3* targetPos = nullptr;
		Vector3 enemyPos{};
		if (context.targetSelection) {
			if (Enemy* lockedEnemy = context.targetSelection->GetLockedEnemy()) {
				enemyPos = lockedEnemy->GetPosition();
				targetPos = &enemyPos;
			}
		}
		context.player->UpdateCamera(context.camera, targetPos);
		context.camera->Update();
	} else if (context.debugFlyCamera) {
		context.debugFlyCamera->Camera::Update();
	}

	Camera* activeCamera = context.isDebugCameraActive
		? static_cast<Camera*>(context.debugFlyCamera) : context.camera;
	if (context.player) {
		context.player->UpdateModel();
	}
	if (context.enemies) {
		for (auto& enemy : *context.enemies) {
			enemy->UpdateModel();
		}
	}
	if (context.obstacles) {
		for (auto& obstacle : *context.obstacles) {
			obstacle->Update();
		}
	}
	if (context.missileManager) {
		context.missileManager->UpdateModels(activeCamera);
	}

	setStatus("設定「" + trimmedName + "」をゲームに読み込みました。");
	return true;
}
