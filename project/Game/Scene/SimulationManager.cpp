#include "SimulationManager.h"
#include "MissilePresetManager.h"
#include "LockOnManager.h"
#include "GamePlayScene.h"
#include "GamePlaySceneHelpers.h"
#include <externals/imgui/imgui.h>
#include <fstream>
#include "engine/Input/Input.h"
#include "engine/math/MyMath.h"

#include <filesystem>
#include <Windows.h>
#include <shellapi.h>

namespace {
std::filesystem::path FindProjectRoot() {
	wchar_t modulePath[MAX_PATH] = {};
	if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
		return {};
	}

	std::filesystem::path current = std::filesystem::path(modulePath).parent_path();
	while (!current.empty()) {
		if (std::filesystem::exists(current / "project" / "CG2.sln") &&
			std::filesystem::exists(current / "tools" / "package_release.ps1")) {
			return current;
		}
		const std::filesystem::path parent = current.parent_path();
		if (parent == current) {
			break;
		}
		current = parent;
	}
	return {};
}

bool LaunchExecutablePackageCreation(bool skipBuild) {
	const std::filesystem::path projectRoot = FindProjectRoot();
	if (projectRoot.empty()) {
		return false;
	}

	const std::filesystem::path scriptPath = projectRoot / "tools" / "package_release.ps1";
	std::wstring parameters = L"-NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath.wstring() +
		L"\" -Configuration Release";
	if (skipBuild) {
		parameters += L" -SkipBuild";
	}

	const HINSTANCE result = ShellExecuteW(
		nullptr, L"open", L"powershell.exe", parameters.c_str(), projectRoot.c_str(), SW_SHOWNORMAL);
	return reinterpret_cast<intptr_t>(result) > 32;
}

bool IsCurrentExecutableReleaseBuild() {
	wchar_t modulePath[MAX_PATH] = {};
	if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
		return false;
	}
	const std::filesystem::path projectRoot = FindProjectRoot();
	if (projectRoot.empty()) {
		return false;
	}
	const std::filesystem::path releaseExe = projectRoot / "generated" / "outputs" / "Release" / "ValkyrieStrike.exe";
	return std::filesystem::path(modulePath).lexically_normal() == releaseExe.lexically_normal();
}
}

SimulationManager::SimulationManager(GamePlayScene* scene) : scene_(scene) {
	RefreshActionAnimationsList();
}

bool SimulationManager::SaveCurrentSimulationLayoutToSceneJson(const std::string &filePath) {
	json root;
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) {
			scene_->uiManager_->simulationSaveMessage_ = "scene.json が見つからないため保存できませんでした";
			OutputDebugStringA(("[SimulationSave] File not found: " + filePath + "\n").c_str());
			return false;
		}

		try {
			ifs >> root;
		} catch (const std::exception &e) {
			scene_->uiManager_->simulationSaveMessage_ = "scene.json の読み込みに失敗しました";
			OutputDebugStringA(("[SimulationSave] JSON parse failed: " + std::string(e.what()) + "\n").c_str());
			return false;
		}
	}

	if (!root.contains("objects") || !root["objects"].is_array()) {
		scene_->uiManager_->simulationSaveMessage_ = "scene.json に objects がないため保存できませんでした";
		OutputDebugStringA("[SimulationSave] objects array not found.\n");
		return false;
	}

	json &objects = root["objects"];
	size_t playerObjectIndex = kInvalidSceneObjectIndex;
	std::vector<size_t> enemyObjectIndices;
	std::vector<size_t> obstacleObjectIndices;

	for (size_t index = 0; index < objects.size(); ++index) {
		const json &objectData = objects[index];
		if (IsScenePlayerObject(objectData) && playerObjectIndex == kInvalidSceneObjectIndex) {
			playerObjectIndex = index;
		}
		if (IsSceneEnemyObject(objectData)) {
			enemyObjectIndices.push_back(index);
		} else if (IsSceneObstacleObject(objectData)) {
			obstacleObjectIndices.push_back(index);
		}
	}

	size_t savedCount = 0;

	if (scene_->player_ && playerObjectIndex != kInvalidSceneObjectIndex) {
		const Vector3 playerRotation = EulerFromQuaternionXYZ(scene_->player_->GetQuaternion());
		WriteSceneTransform(objects[playerObjectIndex], scene_->player_->GetPosition(), &playerRotation, nullptr);
		++savedCount;
	}

	size_t obstacleIndex = 0;
	for (const auto &obstacle : scene_->obstacles_) {
		if (!obstacle || obstacleIndex >= obstacleObjectIndices.size()) {
			break;
		}

		const Vector3 obstacleRotation = obstacle->GetRotation();
		const Vector3 obstacleScale = obstacle->GetScale();
		WriteSceneTransform(
			objects[obstacleObjectIndices[obstacleIndex]],
			obstacle->GetPosition(),
			&obstacleRotation,
			&obstacleScale);
		++obstacleIndex;
		++savedCount;
	}

	for (const auto &enemy : scene_->enemies_) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		const Vector3 enemyPosition = enemy->GetPosition();
		const Vector3 enemyRotation = enemy->GetRotation();
		const size_t spawnPointIndex = enemy->GetSpawnPointIndex();

		if (spawnPointIndex != Enemy::kNoSpawnPoint && spawnPointIndex < enemyObjectIndices.size()) {
			WriteSceneTransform(objects[enemyObjectIndices[spawnPointIndex]], enemyPosition, &enemyRotation, nullptr);
			if (spawnPointIndex < scene_->enemySpawns_.size()) {
				scene_->enemySpawns_[spawnPointIndex].position = enemyPosition;
				scene_->enemySpawns_[spawnPointIndex].rotation = enemyRotation;
			}
			++savedCount;
			continue;
		}

		json newEnemy;
		newEnemy["type"] = "MESH";
		newEnemy["name"] = MakeUniqueSceneObjectName(objects, "SimEnemy");
		newEnemy["category"] = "ENEMY";
		newEnemy["enemy"] = { { "type", "VF1" } };
		newEnemy["vertices_count"] = 0;
		const Vector3 enemyScale = enemy->GetScale();
		WriteSceneTransform(newEnemy, enemyPosition, &enemyRotation, &enemyScale);

		objects.push_back(newEnemy);

		EnemySpawnData spawnData;
		spawnData.name = newEnemy["name"].get<std::string>();
		spawnData.position = enemyPosition;
		spawnData.rotation = enemyRotation;
		spawnData.isInitialSpawn = true;
		scene_->enemySpawns_.push_back(spawnData);
		enemy->SetSpawnPointIndex(scene_->enemySpawns_.size() - 1);
		enemyObjectIndices.push_back(objects.size() - 1);
		++savedCount;
	}

	// 増援・ボスなど、現在は未出現の敵もエディタで動かした位置を保存する。
	for (size_t spawnPointIndex = 0;
		spawnPointIndex < scene_->enemySpawns_.size() && spawnPointIndex < enemyObjectIndices.size();
		++spawnPointIndex) {
		const EnemySpawnData &spawn = scene_->enemySpawns_[spawnPointIndex];
		WriteSceneTransform(objects[enemyObjectIndices[spawnPointIndex]], spawn.position, &spawn.rotation, nullptr);
		++savedCount;
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		scene_->uiManager_->simulationSaveMessage_ = "scene.json を書き込めませんでした";
		OutputDebugStringA(("[SimulationSave] Failed to open for write: " + filePath + "\n").c_str());
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	try {
		scene_->lastJsonWriteTime_ = std::filesystem::last_write_time(filePath);
	} catch (...) {
	}

	scene_->uiManager_->simulationSaveMessage_ = "現在の配置を scene.json に保存しました。実ゲームにも反映されます";
	OutputDebugStringA(("[SimulationSave] Saved " + std::to_string(savedCount) + " transforms.\n").c_str());
	return true;
}

bool SimulationManager::AddEnemySpawnToSceneJson(
	const std::string &filePath,
	const std::string &name,
	const std::string &enemyType,
	const Vector3 &position,
	const Vector3 &rotation) {
	json root;
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) {
			scene_->uiManager_->simulationSaveMessage_ = "scene.json が見つからないため敵を追加できませんでした";
			return false;
		}

		try {
			ifs >> root;
		} catch (const std::exception &) {
			scene_->uiManager_->simulationSaveMessage_ = "scene.json の読み込みに失敗したため敵を追加できませんでした";
			return false;
		}
	}

	if (!root.contains("objects") || !root["objects"].is_array()) {
		scene_->uiManager_->simulationSaveMessage_ = "scene.json に objects がないため敵を追加できませんでした";
		return false;
	}

	json &objects = root["objects"];
	json newEnemy;
	newEnemy["type"] = "MESH";
	newEnemy["name"] = MakeUniqueSceneObjectName(objects, name.empty() ? "Enemy" : name);
	newEnemy["category"] = "ENEMY";
	newEnemy["enemy"] = {
		{ "type", enemyType.empty() ? "VF1" : enemyType },
		{ "is_initial_spawn", true },
	};
	newEnemy["vertices_count"] = 0;
	const Vector3 defaultScale = { 1.0f, 1.0f, 1.0f };
	WriteSceneTransform(newEnemy, position, &rotation, &defaultScale);
	objects.push_back(std::move(newEnemy));

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		scene_->uiManager_->simulationSaveMessage_ = "scene.json を書き込めませんでした";
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	try {
		scene_->lastJsonWriteTime_ = std::filesystem::last_write_time(filePath);
	} catch (...) {
	}

	// ファイル保存だけでなく、ゲーム中のプレビューにも即時反映する。
	scene_->ReloadSceneJson();
	scene_->uiManager_->simulationSaveMessage_ = "敵配置を scene.json に保存し、プレビューへ反映しました";
	return true;
}

bool SimulationManager::MoveEnemySpawnInSceneJson(const std::string &filePath, size_t sourceIndex, size_t targetIndex) {
	json root;
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) {
			scene_->uiManager_->simulationSaveMessage_ = "scene.json が見つからないためスポーン順を変更できませんでした";
			return false;
		}

		try {
			ifs >> root;
		} catch (const std::exception &) {
			scene_->uiManager_->simulationSaveMessage_ = "scene.json の読み込みに失敗したためスポーン順を変更できませんでした";
			return false;
		}
	}

	if (!root.contains("objects") || !root["objects"].is_array()) {
		scene_->uiManager_->simulationSaveMessage_ = "scene.json に objects がないためスポーン順を変更できませんでした";
		return false;
	}

	json &objects = root["objects"];
	std::vector<size_t> enemyObjectIndices;
	for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
		if (IsSceneEnemyObject(objects[objectIndex])) {
			enemyObjectIndices.push_back(objectIndex);
		}
	}

	if (sourceIndex >= enemyObjectIndices.size() || targetIndex >= enemyObjectIndices.size() || sourceIndex == targetIndex) {
		return false;
	}

	std::swap(objects[enemyObjectIndices[sourceIndex]], objects[enemyObjectIndices[targetIndex]]);
	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		scene_->uiManager_->simulationSaveMessage_ = "scene.json を書き込めませんでした";
		return false;
	}
	ofs << root.dump(4);
	ofs.close();

	try {
		scene_->lastJsonWriteTime_ = std::filesystem::last_write_time(filePath);
	} catch (...) {
	}

	scene_->ReloadSceneJson();
	scene_->uiManager_->simulationSaveMessage_ = "敵の初期スポーン順を変更し、プレビューへ反映しました";
	return true;
}

void SimulationManager::RefreshSimulationActionNames() {
	scene_->uiManager_->simulationActionNames_.clear();

	std::ifstream ifs(kSimulationActionsFilePath);
	if (!ifs.is_open()) {
		scene_->uiManager_->selectedSimulationActionIndex_ = 0;
		return;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		scene_->uiManager_->selectedSimulationActionIndex_ = 0;
		return;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		scene_->uiManager_->selectedSimulationActionIndex_ = 0;
		return;
	}

	for (const auto &actionData : root["actions"]) {
		if (!actionData.is_object() || !actionData.contains("name") || !actionData["name"].is_string()) {
			continue;
		}

		scene_->uiManager_->simulationActionNames_.push_back(actionData["name"].get<std::string>());
	}

	if (scene_->uiManager_->selectedSimulationActionIndex_ >= static_cast<int>(scene_->uiManager_->simulationActionNames_.size())) {
		scene_->uiManager_->selectedSimulationActionIndex_ = scene_->uiManager_->simulationActionNames_.empty() ? 0 : static_cast<int>(scene_->uiManager_->simulationActionNames_.size()) - 1;
	}
}

bool SimulationManager::SaveNamedSimulationAction(const std::string &filePath, const std::string &actionName) {
	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		scene_->uiManager_->simulationActionMessage_ = "保存名を入力してください。";
		return false;
	}

	json root;
	{
		std::ifstream ifs(filePath);
		if (ifs.is_open()) {
			try {
				ifs >> root;
			} catch (...) {
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

	if (scene_->player_) {
		json playerData;
		playerData["position"] = ToVector3Json(scene_->player_->GetPosition());
		playerData["rotation"] = ToVector3Json(EulerFromQuaternionXYZ(scene_->player_->GetQuaternion()));
		playerData["mode"] = static_cast<int>(scene_->player_->GetCurrentMode());

		json modeParams = json::array();
		for (int modeIndex = 0; modeIndex < 3; ++modeIndex) {
			modeParams.push_back(PlayerModeParamsToJson(scene_->player_->GetModeParams(static_cast<PlayerMode>(modeIndex))));
		}
		playerData["modeParams"] = modeParams;
		action["player"] = playerData;
	}

	json missileData;
	missileData["normalSpeed"] = scene_->missileNormalSpeed;
	missileData["normalScale"] = scene_->missileNormalScale;
	missileData["normalCollisionRadius"] = scene_->missileNormalCollisionRadius;
	missileData["normalLifeTime"] = scene_->missileNormalLifeTime;
	missileData["speed"] = scene_->missileSpeed;
	missileData["ampX"] = scene_->missileAmpX;
	missileData["ampZ"] = scene_->missileAmpZ;
	missileData["ampY"] = scene_->missileAmpY;
	missileData["freqY"] = scene_->missileFreqY;
	missileData["baseY"] = scene_->missileBaseY;
	missileData["homingStrength"] = scene_->missileHomingStrength;
	missileData["homingScale"] = scene_->missileHomingScale;
	missileData["homingCollisionRadius"] = scene_->missileHomingCollisionRadius;
	missileData["trailWidth"] = scene_->missileTrailWidth;
	missileData["lifeTime"] = scene_->missileLifeTime;
	missileData["muzzleOffset"] = scene_->missileMuzzleOffset;
	action["missile"] = missileData;

	json enemiesData = json::array();
	int generatedEnemyIndex = 1;
	for (const auto &enemy : scene_->enemies_) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		json enemyData;
		const size_t spawnPointIndex = enemy->GetSpawnPointIndex();
		if (spawnPointIndex < scene_->enemySpawns_.size()) {
			const EnemySpawnData &spawnData = scene_->enemySpawns_[spawnPointIndex];
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
	action["enemies"] = enemiesData;

	json obstaclesData = json::array();
	for (const auto &obstacle : scene_->obstacles_) {
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
	action["obstacles"] = obstaclesData;

	if (scene_->explosionManager_) {
		const auto &config = scene_->explosionManager_->GetConfig();
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
		action["explosion"] = explosionData;
	}

	json &actions = root["actions"];
	bool updated = false;
	for (auto &existingAction : actions) {
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
		scene_->uiManager_->simulationActionMessage_ = "名前付き行動を保存できませんでした。";
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	RefreshSimulationActionNames();
	for (size_t index = 0; index < scene_->uiManager_->simulationActionNames_.size(); ++index) {
		if (scene_->uiManager_->simulationActionNames_[index] == trimmedName) {
			scene_->uiManager_->selectedSimulationActionIndex_ = static_cast<int>(index);
			break;
		}
	}

	scene_->uiManager_->simulationActionMessage_ = "行動「" + trimmedName + "」を保存しました。";
	return true;
}

bool SimulationManager::ApplySimulationAction(const std::string &filePath, const std::string &actionName) {
	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		scene_->uiManager_->simulationActionMessage_ = "読み込む行動名がありません。";
		return false;
	}

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		scene_->uiManager_->simulationActionMessage_ = "名前付き行動ファイルが見つかりません。";
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		scene_->uiManager_->simulationActionMessage_ = "名前付き行動ファイルを読み込めませんでした。";
		return false;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		scene_->uiManager_->simulationActionMessage_ = "保存された行動がありません。";
		return false;
	}

	const json *action = nullptr;
	for (const auto &actionData : root["actions"]) {
		if (actionData.is_object() && actionData.value("name", "") == trimmedName) {
			action = &actionData;
			break;
		}
	}

	if (!action) {
		scene_->uiManager_->simulationActionMessage_ = "選択した行動が見つかりません。";
		return false;
	}

	scene_->lockedEnemy_ = nullptr;
	scene_->aimAssistEnemy_ = nullptr;
	scene_->lockOnManager_->CancelMultiLock();
	scene_->isCinematicLockOnCameraInitialized_ = false;
	scene_->isGameOver_ = false;
	scene_->gameOverTimer_ = 0;

	if (scene_->player_ && action->contains("player") && (*action)["player"].is_object()) {
		const json &playerData = (*action)["player"];
		if (playerData.contains("modeParams") && playerData["modeParams"].is_array()) {
			const json &modeParams = playerData["modeParams"];
			for (int modeIndex = 0; modeIndex < 3 && modeIndex < static_cast<int>(modeParams.size()); ++modeIndex) {
				ApplyPlayerModeParamsFromJson(modeParams[modeIndex], scene_->player_->GetModeParams(static_cast<PlayerMode>(modeIndex)));
			}
		}

		const int modeIndex = std::clamp(ReadJsonInt(playerData, "mode", 0), 0, 2);
		scene_->player_->ChangeMode(static_cast<PlayerMode>(modeIndex));
		scene_->player_->SetPosition(ReadVector3Json(playerData.value("position", json::array()), scene_->player_->GetPosition()));
		scene_->player_->SetRotation(ReadVector3Json(playerData.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f }));
	}

	if (action->contains("missile") && (*action)["missile"].is_object()) {
		const json &missileData = (*action)["missile"];
		scene_->missileNormalSpeed = ReadJsonFloat(missileData, "normalSpeed", scene_->missileNormalSpeed);
		scene_->missileNormalScale = ReadJsonFloat(missileData, "normalScale", scene_->missileNormalScale);
		scene_->missileNormalCollisionRadius = ReadJsonFloat(missileData, "normalCollisionRadius", scene_->missileNormalCollisionRadius);
		scene_->missileNormalLifeTime = ReadJsonInt(missileData, "normalLifeTime", scene_->missileNormalLifeTime);
		scene_->missileSpeed = ReadJsonFloat(missileData, "speed", scene_->missileSpeed);
		scene_->missileAmpX = ReadJsonFloat(missileData, "ampX", scene_->missileAmpX);
		scene_->missileAmpZ = ReadJsonFloat(missileData, "ampZ", scene_->missileAmpZ);
		scene_->missileAmpY = ReadJsonFloat(missileData, "ampY", scene_->missileAmpY);
		scene_->missileFreqY = ReadJsonFloat(missileData, "freqY", scene_->missileFreqY);
		scene_->missileBaseY = ReadJsonFloat(missileData, "baseY", scene_->missileBaseY);
		scene_->missileHomingStrength = ReadJsonFloat(missileData, "homingStrength", scene_->missileHomingStrength);
		scene_->missileHomingScale = ReadJsonFloat(missileData, "homingScale", scene_->missileHomingScale);
		scene_->missileHomingCollisionRadius = ReadJsonFloat(missileData, "homingCollisionRadius", scene_->missileHomingCollisionRadius);
		scene_->missileTrailWidth = ReadJsonFloat(missileData, "trailWidth", scene_->missileTrailWidth);
		scene_->missileLifeTime = ReadJsonInt(missileData, "lifeTime", scene_->missileLifeTime);
		scene_->missileMuzzleOffset = ReadJsonFloat(missileData, "muzzleOffset", scene_->missileMuzzleOffset);
	}

	if (scene_->explosionManager_ && action->contains("explosion") && (*action)["explosion"].is_object()) {
		const json &explosionData = (*action)["explosion"];
		auto &config = scene_->explosionManager_->GetConfig();
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
	}

	if (action->contains("obstacles") && (*action)["obstacles"].is_array()) {
		auto obstacleIt = scene_->obstacles_.begin();
		for (const auto &obstacleData : (*action)["obstacles"]) {
			if (obstacleIt == scene_->obstacles_.end()) {
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

	if (action->contains("enemies") && (*action)["enemies"].is_array()) {
		scene_->enemies_.clear();
		scene_->enemySpawns_.clear();

		for (const auto &enemyData : (*action)["enemies"]) {
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
			scene_->enemySpawns_.push_back(spawnData);

			if (spawnData.isInitialSpawn) {
				auto enemy = std::make_unique<Enemy>();
				enemy->Initialize(spawnData.position);
				enemy->SetRotation(spawnData.rotation);
				enemy->SetScale(ReadVector3Json(enemyData.value("scale", json::array()), enemy->GetScale()));
				enemy->SetSpawnPointIndex(scene_->enemySpawns_.size() - 1);
				scene_->enemies_.push_back(std::move(enemy));
			}
		}
		scene_->enemyRespawnTimers_.assign(scene_->enemySpawns_.size(), kNoRespawnTimer);
	}

	if (scene_->missileManager_) {
		scene_->missileManager_->Initialize(scene_->environmentRenderer_->GetParticleManager());
	}
	if (scene_->enemyBulletManager_) {
		scene_->enemyBulletManager_->Initialize();
	}

	if (!scene_->isDebugCameraActive_ && scene_->camera && scene_->player_) {
		Vector3 *targetPos = nullptr;
		Vector3 enemyPos;
		if (scene_->lockedEnemy_) {
			enemyPos = scene_->lockedEnemy_->GetPosition();
			targetPos = &enemyPos;
		}
		scene_->player_->UpdateCamera(scene_->camera.get(), targetPos);
		scene_->camera->Update();
	} else if (scene_->debugFlyCamera_) {
		scene_->debugFlyCamera_->Camera::Update();
	}

	Camera *activeCamera = scene_->isDebugCameraActive_ ? static_cast<Camera *>(scene_->debugFlyCamera_.get()) : scene_->camera.get();
	if (scene_->player_) {
		scene_->player_->UpdateModel();
	}
	for (auto &enemy : scene_->enemies_) {
		enemy->UpdateModel();
	}
	for (auto &obstacle : scene_->obstacles_) {
		obstacle->Update();
	}
	if (scene_->missileManager_) {
		scene_->missileManager_->UpdateModels(activeCamera);
	}

	scene_->uiManager_->simulationActionMessage_ = "設定「" + trimmedName + "」をゲームに読み込みました。";
	return true;
}

void SimulationManager::DrawSimulationScreenUI() {
#ifdef ENABLE_IMGUI
	const ImVec2 windowSize(
		static_cast<float>(WinApp::GetClientWidth()),
		static_cast<float>(WinApp::GetClientHeight()));
	float panelWidth = (windowSize.x > 560.0f) ? 520.0f : windowSize.x - 40.0f;
	if (panelWidth < 320.0f) {
		panelWidth = 320.0f;
	}
	float panelHeight = windowSize.y - 40.0f;
	if (panelHeight < 360.0f) {
		panelHeight = 360.0f;
	}
	float panelX = windowSize.x - panelWidth - 20.0f;
	if (panelX < 20.0f) {
		panelX = 20.0f;
	}
	ImGui::SetNextWindowPos(ImVec2(panelX, 20.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_FirstUseEver);

	const ImGuiWindowFlags windowFlags = 0;

	if (!ImGui::Begin("シミュレーション画面", nullptr, windowFlags)) {
		ImGui::End();
		return;
	}

	ImGui::Text("シミュレーション画面");
	ImGui::SameLine();
	if (ImGui::Button("シミュレーションを閉じる (F2)")) {
		PostQuitMessage(0);
		ImGui::End();
		return;
	}
	ImGui::SameLine();
	if (ImGui::Button("リセット")) {
		scene_->ResetEditorPreview();
	}
	ImGui::SameLine();
	if (ImGui::Button("再生")) {
		scene_->isEditorPreviewPlaying_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("停止")) {
		scene_->isEditorPreviewPlaying_ = false;
	}
	ImGui::SameLine();
	ImGui::TextColored(
		scene_->isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
		"状態: %s",
		scene_->isEditorPreviewPlaying_ ? "再生中" : "停止中");

	ImGui::Separator();
	const char *previewModes[] = { "選択中だけ確認", "全体確認" };
	ImGui::Combo("確認モード", &scene_->uiManager_->simulationPlaybackMode_, previewModes, IM_ARRAYSIZE(previewModes));
	if (scene_->uiManager_->simulationPlaybackMode_ == 0) {
		ImGui::TextDisabled("今選んでいるカテゴリだけ動きます。ミサイルはテスト発射ボタンでだけ出ます。");
	} else {
		ImGui::TextDisabled("プレイヤー・敵・ミサイルをまとめて動かして全体の流れを確認します。");
	}

	ImGui::Separator();
	ImGui::Text("カメラ: %s", scene_->isDebugCameraActive_ ? "フリーカメラ" : "プレイヤー視点");
	ImGui::SameLine();
	if (scene_->isDebugCameraActive_) {
		if (ImGui::Button("プレイヤー視点にする (F3)")) {
			scene_->SetDebugCameraActive(false);
		}
	} else {
		if (ImGui::Button("フリーカメラに戻す (F3)")) {
			scene_->SetDebugCameraActive(true);
		}
	}

	DrawSimulationSaveControls();

	ImGui::Separator();
		const char *categories[] = { "プレイヤー", "ミサイル", "敵 & イベント", "パーティクル", "カメラ", "アニメーション編集", "操作設定", "実行ファイル生成" };

	// 既存の設定を、左の一覧から選択して右側で編集するレイアウトにする。
	// カテゴリごとの実際の操作処理は下の既存コードをそのまま使用する。
	ImGui::BeginChild("設定一覧", ImVec2(170.0f, 0.0f), true);
	ImGui::TextDisabled("設定一覧");
	ImGui::Separator();
	for (int categoryIndex = 0; categoryIndex < IM_ARRAYSIZE(categories); ++categoryIndex) {
		const bool isSelected = scene_->uiManager_->currentSimulationTarget_ == categoryIndex;
		if (ImGui::Selectable(categories[categoryIndex], isSelected, 0, ImVec2(0.0f, 32.0f))) {
			scene_->uiManager_->currentSimulationTarget_ = categoryIndex;
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("設定内容", ImVec2(0.0f, 0.0f), true);
	ImGui::Text("%s の設定", categories[scene_->uiManager_->currentSimulationTarget_]);
	ImGui::Separator();

	if (scene_->uiManager_->currentSimulationTarget_ == 0) {
		ImGui::Text("プレイヤー移動設定");
		if (scene_->player_) {
			auto mode = scene_->player_->GetCurrentMode();
			const char* modeName = (mode == PlayerMode::Fighter) ? "ファイター (1キー)" : 
								   (mode == PlayerMode::Gerwalk) ? "ガウォーク (2キー)" : "バトロイド (3キー)";
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "現在の形態: %s", modeName);
			
			PlayerModeParams& p = scene_->player_->GetModeParams(mode);
			ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
			ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
			ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
			ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);

			ImGui::Separator();
			ImGui::Text("アニメーションデバッグ");
			bool isDebug = scene_->player_->IsAnimDebugActive();
			if (ImGui::Checkbox("デバッグ時間を強制", &isDebug)) {
				scene_->player_->SetAnimDebugActive(isDebug);
			}
			if (isDebug) {
				float t = scene_->player_->GetTargetAnimationTime();
				float duration = scene_->player_->GetAnimationDuration();
				if (ImGui::SliderFloat("アニメーション時間 (秒)", &t, 0.0f, duration, "%.3f秒")) {
					scene_->player_->SetTargetAnimationTime(t);
				}
				ImGui::Text("対応フレーム (24fps換算): %.1f", t * 24.0f);
			} else {
				ImGui::Text("アニメーション時間: %.3f秒 (目標: %.3f秒)", scene_->player_->GetAnimationTime(), scene_->player_->GetTargetAnimationTime());
			}
		} else {
			ImGui::Text("プレイヤーが初期化されていません。");
		}
	} else if (scene_->uiManager_->currentSimulationTarget_ == 1) {
		scene_->missilePresetManager_->DrawMissileSettingsUI();
	} else if (scene_->uiManager_->currentSimulationTarget_ == 2) {
		ImGui::Text("=== 敵の出現とルート ===");
		ImGui::Text("Lock-on: %s", scene_->lockedEnemy_ ? "LOCKED" : "NONE");
		ImGui::Text("Tab: ターゲットロック / X: ロック解除 / F2: シミュレーションを閉じる");
		ImGui::DragFloat3("出現座標 (X,Y,Z)", scene_->newEnemyPos, 1.0f);

		if (ImGui::Button("敵を生成する！")) {
			auto newEnemy = std::make_unique<Enemy>();
			newEnemy->Initialize({ scene_->newEnemyPos[0], scene_->newEnemyPos[1], scene_->newEnemyPos[2] });
			scene_->enemies_.push_back(std::move(newEnemy));
		}

		ImGui::Separator();
		ImGui::Text("敵のリスト (総数: %d)", static_cast<int>(scene_->enemies_.size()));
		int index = 0;
		for (const auto &enemy : scene_->enemies_) {
			Vector3 pos = enemy->GetPosition();
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
			++index;
		}

		ImGui::Separator();
		ImGui::Text("イベントツール（増援設定）");
		static int selectedTrigger = 0;
		static int selectedTarget = 0;
		static int eventDelay = 60;
		std::vector<const char *> enemyNames;
		for (const auto &spawn : scene_->enemySpawns_) {
			enemyNames.push_back(spawn.name.c_str());
		}

		if (!enemyNames.empty()) {
			if (selectedTrigger >= static_cast<int>(enemyNames.size())) {
				selectedTrigger = 0;
			}
			if (selectedTarget >= static_cast<int>(enemyNames.size())) {
				selectedTarget = 0;
			}

			ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);
			if (ImGui::Button("イベントを追加")) {
				scene_->enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
				scene_->enemySpawns_[selectedTarget].isInitialSpawn = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("イベントを保存")) {
				scene_->enemyEventManager_.SaveEvents("resources/enemy_events.json");
			}

			for (size_t i = 0; i < scene_->enemyEventManager_.GetEvents().size(); ++i) {
				const auto &event = scene_->enemyEventManager_.GetEvents()[i];
				ImGui::Text("[%d] %s が死んだら %d F後に %s が出現",
					static_cast<int>(i),
					event.triggerEnemyName.c_str(),
					event.delayFrames,
					event.targetEnemyName.c_str());
				ImGui::SameLine();
				if (ImGui::Button(("削除##event" + std::to_string(i)).c_str())) {
					scene_->enemyEventManager_.RemoveEvent(i);
					break;
				}
			}
		} else {
			ImGui::Text("敵の出現データがありません。");
		}
	} else if (scene_->uiManager_->currentSimulationTarget_ == 3) {
		ImGui::Text("=== GPUパーティクル ===");
		bool gpuChanged = false;
		if (scene_->environmentRenderer_->GetParticleManager()) {
			if (auto *emitter = scene_->environmentRenderer_->GetParticleManager()->GetEmitterSphere()) {
				if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
				if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
				if (ImGui::DragInt("射出数", reinterpret_cast<int *>(&emitter->count), 1, 0, 1000)) gpuChanged = true;
				if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
			}
			if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
				scene_->environmentRenderer_->GetParticleManager()->RequestGpuInitialize();
			}
		}

		ImGui::Separator();
		ImGui::Text("=== 爆発パーティクル ===");
		if (scene_->explosionManager_) {
			auto &config = scene_->explosionManager_->GetConfig();
			ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
			ImGui::ColorEdit4("カラー", config.color);
			ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
			ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
			ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
			ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);
			if (ImGui::Button("設定をJSONに保存")) {
				scene_->explosionManager_->SaveToJson("resources/explosionConfig.json");
			}
			ImGui::SameLine();
			if (ImGui::Button("設定をJSONから読込")) {
				scene_->explosionManager_->LoadFromJson("resources/explosionConfig.json");
			}
		}
	} else if (scene_->uiManager_->currentSimulationTarget_ == 4) {
		ImGui::Text("カメラ設定");
		if (scene_->isDebugCameraActive_) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[フリーカメラ アクティブ]");
			if (ImGui::Button("プレイヤー視点にする")) {
				scene_->SetDebugCameraActive(false);
			}
			float moveSpeed = scene_->debugFlyCamera_->GetMoveSpeed();
			float rotateSpeed = scene_->debugFlyCamera_->GetRotateSpeed();
			float sensitivity = scene_->debugFlyCamera_->GetMouseSensitivity();
			float scrollSpeed = scene_->debugFlyCamera_->GetScrollSpeed();
			float panSpeed = scene_->debugFlyCamera_->GetPanSpeed();
			if (ImGui::DragFloat("移動速度 (WASD)##fly", &moveSpeed, 0.01f, 0.01f, 20.0f)) scene_->debugFlyCamera_->SetMoveSpeed(moveSpeed);
			if (ImGui::DragFloat("回転感度 (マウス右)##fly", &sensitivity, 0.0001f, 0.0001f, 0.05f, "%.4f")) scene_->debugFlyCamera_->SetMouseSensitivity(sensitivity);
			if (ImGui::Button("ステージ全体を表示")) {
				scene_->ResetDebugCameraToStageOverview();
			}
		scene_->debugFlyCamera_->SetRotateSpeed(rotateSpeed);

			Vector3 flyPos = scene_->debugFlyCamera_->GetTranslate();
			float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
			if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) {
				scene_->debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
			}
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[プレイヤー視点 アクティブ]");
			if (ImGui::Button("フリーカメラに切り替え")) {
				scene_->SetDebugCameraActive(true);
			}
			if (ImGui::Checkbox("Cinematic lock-on scene_->camera", &scene_->isCinematicLockOnCameraEnabled_)) {
				scene_->isCinematicLockOnCameraInitialized_ = false;
			}
			ImGui::Text("Cinematic: %s", (scene_->isCinematicLockOnCameraEnabled_ && scene_->lockedEnemy_) ? "ACTIVE" : "OFF");
		}
	} else if (scene_->uiManager_->currentSimulationTarget_ == 5) {
		DrawAnimationEditorUI();
	} else if (scene_->uiManager_->currentSimulationTarget_ == 6) {
		Input *input = Input::GetInstance();
		ImGui::Text("プレイヤー操作設定");
		ImGui::TextDisabled("変更を押した後、キーボードまたはマウスボタンを押して割り当てます。設定は resources/input_bindings.cfg に保存されます。");
		ImGui::TextColored(
			input->IsControllerConnected() ? ImVec4(0.25f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.70f, 0.25f, 1.0f),
			"コントローラー: %s (XInput Player 1)", input->IsControllerConnected() ? "接続済み" : "未接続");
		if (ImGui::Button("初期設定に戻す")) {
			input->ResetPlayerActionBindings();
			input->SavePlayerActionBindings();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("キーボード・コントローラー・マウスは同時に使用できます。");
		ImGui::Separator();

		static int waitingForKeyboardAction = -1;
		static bool ignoreMouseCaptureUntilReleased = false;
		for (size_t actionIndex = 0; actionIndex < Input::GetPlayerActionCount(); ++actionIndex) {
			const PlayerAction action = static_cast<PlayerAction>(actionIndex);
			const PlayerActionBinding &binding = input->GetActionBinding(action);
			ImGui::PushID(static_cast<int>(actionIndex));
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", Input::GetPlayerActionName(action));
			ImGui::SameLine(175.0f);
			if (waitingForKeyboardAction == static_cast<int>(actionIndex)) {
				ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.20f, 1.0f), "キーまたはマウスを押してください (Esc: キャンセル)");
				for (int keyNumber = 1; keyNumber < 256; ++keyNumber) {
					if (!input->TriggerKey(static_cast<BYTE>(keyNumber))) {
						continue;
					}
					if (keyNumber != DIK_ESCAPE) {
						input->SetKeyboardBinding(action, static_cast<BYTE>(keyNumber));
						input->SavePlayerActionBindings();
					}
					waitingForKeyboardAction = -1;
					break;
				}

				const bool mouseReleased = !input->PushMouseButton(0) && !input->PushMouseButton(1) &&
					!input->PushMouseButton(2) && !input->PushMouseButton(3);
				if (ignoreMouseCaptureUntilReleased && mouseReleased) {
					ignoreMouseCaptureUntilReleased = false;
				}
				if (waitingForKeyboardAction == static_cast<int>(actionIndex) && !ignoreMouseCaptureUntilReleased) {
					for (int button = 0; button < 4; ++button) {
						if (!input->TriggerMouseButton(button)) {
							continue;
						}
						input->SetMouseBinding(action, static_cast<MouseInput>(static_cast<int>(MouseInput::LeftButton) + button));
						input->SavePlayerActionBindings();
						waitingForKeyboardAction = -1;
						break;
					}
					if (waitingForKeyboardAction == static_cast<int>(actionIndex)) {
						if (input->GetMouseWheel() > 0) {
							input->SetMouseBinding(action, MouseInput::WheelUp);
							input->SavePlayerActionBindings();
							waitingForKeyboardAction = -1;
						} else if (input->GetMouseWheel() < 0) {
							input->SetMouseBinding(action, MouseInput::WheelDown);
							input->SavePlayerActionBindings();
							waitingForKeyboardAction = -1;
						}
					}
				}
			} else {
				const std::string keyLabel = Input::GetKeyboardKeyName(binding.keyboardKey);
				ImGui::Text("キー: %s", keyLabel.c_str());
				ImGui::SameLine();
				if (ImGui::Button("キー変更")) {
					waitingForKeyboardAction = static_cast<int>(actionIndex);
					// 「キー変更」を押した左クリックを、直後に割り当ててしまわないようにする。
					ignoreMouseCaptureUntilReleased = true;
				}
			}

			ImGui::SameLine(360.0f);
			ImGui::SetNextItemWidth(145.0f);
			const char *controllerLabel = Input::GetControllerInputName(binding.controllerInput);
			if (ImGui::BeginCombo("##controller", controllerLabel)) {
				for (size_t controllerIndex = 0; controllerIndex < Input::GetControllerInputCount(); ++controllerIndex) {
					const ControllerInput controllerInput = static_cast<ControllerInput>(controllerIndex);
					const bool selected = controllerInput == binding.controllerInput;
					if (ImGui::Selectable(Input::GetControllerInputName(controllerInput), selected)) {
						input->SetControllerBinding(action, controllerInput);
						input->SavePlayerActionBindings();
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::TextDisabled("マウス:");
			ImGui::SameLine(360.0f);
			ImGui::SetNextItemWidth(145.0f);
			const char *mouseLabel = Input::GetMouseInputName(binding.mouseInput);
			if (ImGui::BeginCombo("##mouse", mouseLabel)) {
				for (size_t mouseIndex = 0; mouseIndex < Input::GetMouseInputCount(); ++mouseIndex) {
					const MouseInput mouseInput = static_cast<MouseInput>(mouseIndex);
					const bool selected = mouseInput == binding.mouseInput;
					if (ImGui::Selectable(Input::GetMouseInputName(mouseInput), selected)) {
						input->SetMouseBinding(action, mouseInput);
						input->SavePlayerActionBindings();
					}
					if (selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
		}
	} else if (scene_->uiManager_->currentSimulationTarget_ == 7) {
		static std::string packageMessage;
		const bool projectFound = !FindProjectRoot().empty();
		const bool runningRelease = IsCurrentExecutableReleaseBuild();
		ImGui::Text("実行ファイル生成");
		ImGui::TextWrapped("Release版の CG2.exe、実行用DLL、resources をまとめ、配布用ZIPを作成します。");
		ImGui::Separator();
		if (!projectFound) {
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "プロジェクトフォルダー内から起動した場合のみ使用できます。");
		} else {
			if (runningRelease) {
				ImGui::TextDisabled("現在Release版を実行中のため、実行ファイルを上書きするビルドはできません。");
				if (ImGui::Button("現在のRelease版をZIP作成")) {
					packageMessage = LaunchExecutablePackageCreation(true)
						? "PowerShellでZIP作成を開始しました。完了後、generated/packages を確認してください。"
						: "ZIP作成を開始できませんでした。";
				}
			} else if (ImGui::Button("ReleaseビルドしてZIP作成")) {
				packageMessage = LaunchExecutablePackageCreation(false)
					? "PowerShellでReleaseビルドとZIP作成を開始しました。完了後、generated/packages を確認してください。"
					: "実行ファイル生成を開始できませんでした。";
			}
			ImGui::TextDisabled("出力先: generated/packages/CG2_Playable-Release.zip");
		}
		if (!packageMessage.empty()) {
			ImGui::Spacing();
			ImGui::TextWrapped("%s", packageMessage.c_str());
		}
	}

	ImGui::EndChild();
	ImGui::End();
#endif
}

void SimulationManager::DrawSimulationSaveControls() {
#ifdef ENABLE_IMGUI
	ImGui::Separator();
	ImGui::InputText("保存名", scene_->uiManager_->simulationActionName_, IM_ARRAYSIZE(scene_->uiManager_->simulationActionName_));
	if (ImGui::Button("名前を付けて行動を保存")) {
		SaveNamedSimulationAction(kSimulationActionsFilePath, scene_->uiManager_->simulationActionName_);
	}
	ImGui::SameLine();
	if (ImGui::Button("保存一覧を更新")) {
		RefreshSimulationActionNames();
	}

	if (!scene_->uiManager_->simulationActionMessage_.empty()) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.45f, 1.0f), "%s", scene_->uiManager_->simulationActionMessage_.c_str());
	}
#endif
}
void SimulationManager::RefreshActionAnimationsList() {
	availableActionAnimations_.clear();
	std::string path = "resources/animations";
	if (std::filesystem::exists(path)) {
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			if (entry.path().extension() == ".bana") {
				availableActionAnimations_.push_back(entry.path().stem().string());
			}
		}
	}
}


void SimulationManager::DrawAnimationEditorUI() {
#ifdef ENABLE_IMGUI
	ImGui::Text("=== アニメーション編集ツール ===");
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "[Shortcuts] Space:Play/Pause | Esc:Deselect | Del:Clear Key | Ctrl+Z/Y:Undo/Redo");
	
	if (!scene_->player_) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "プレイヤーがいません");
		return;
	}

	ImGui::InputText("アクション名", actionName_, IM_ARRAYSIZE(actionName_));
	std::string currentPath = "resources/animations/" + std::string(actionName_) + ".bana";
	
	if (ImGui::Button("バイナリからロード")) {
		if (LoadAnimationFromBinary(customAnimation_, currentPath)) {
			scene_->player_->SetOverrideAnimation(&customAnimation_);
			scene_->player_->SetAnimDebugActive(true);
		} else {
			OutputDebugStringA(("Failed to load animation binary: " + currentPath + "\n").c_str());
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("バイナリへ保存")) {
		if (SaveAnimationToBinary(customAnimation_, currentPath)) {
			RefreshActionAnimationsList();
		} else {
			OutputDebugStringA(("Failed to save animation binary: " + currentPath + "\n").c_str());
		}
	}

	if (!availableActionAnimations_.empty()) {
		if (ImGui::BeginCombo("既存ファイル", "")) {
			for (const auto& name : availableActionAnimations_) {
				if (ImGui::Selectable(name.c_str())) {
					strcpy_s(actionName_, name.c_str());
				}
			}
			ImGui::EndCombo();
		}
	}


	ImGui::Separator();
	ImGui::Checkbox("再生する", &isAnimationPlaying_);
	ImGui::SameLine();
	ImGui::Checkbox("ループ", &isLooping_);

	if (customAnimation_.duration <= 0.0f) {
		ImGui::Text("カスタムアニメーションがロードされていません");
		if (ImGui::Button("現在の状態から新規作成")) {
			customAnimation_ = Animation();
			customAnimation_.duration = 2.0f; // 2秒の空アニメーション
			scene_->player_->SetOverrideAnimation(&customAnimation_);
			scene_->player_->SetAnimDebugActive(true);
		}
		return;
	}

	ImGui::SliderFloat("全体の尺 (秒)", &customAnimation_.duration, 0.1f, 10.0f);

	// 編集モード停止中は、アニメーション編集のタイムラインも進めない。
	if (isAnimationPlaying_ && scene_->isEditorPreviewPlaying_) {
		currentFrameTime_ += 1.0f / 60.0f;
		if (currentFrameTime_ > customAnimation_.duration) {
			currentFrameTime_ = isLooping_ ? 0.0f : customAnimation_.duration;
			if (!isLooping_) isAnimationPlaying_ = false;
		}
	}
	ImGui::SliderFloat("タイムライン", &currentFrameTime_, 0.0f, customAnimation_.duration, "%.3f秒");
	scene_->player_->SetTargetAnimationTime(currentFrameTime_);

	ImGui::Separator();
	ImGui::Text("ボーン選択とキーフレーム編集");

	// 複数選択用の表示文字列を作成
	std::string selectedNamesStr = "";
	if (selectedBoneNames_.empty()) {
		selectedNamesStr = "(未選択)";
	} else if (selectedBoneNames_.size() == 1) {
		selectedNamesStr = *selectedBoneNames_.begin();
	} else {
		selectedNamesStr = std::to_string(selectedBoneNames_.size()) + "個のボーンを選択中";
	}

	// 簡略化のためノード一覧をドロップダウンで表示（コンボ内では複数選択可能）
	if (ImGui::BeginCombo("編集するボーン", selectedNamesStr.c_str())) {
		// すでにキーがあるノード
		for (const auto& pair : customAnimation_.nodeAnimations) {
			bool is_selected = (selectedBoneNames_.count(pair.first) > 0);
			if (ImGui::Selectable(pair.first.c_str(), is_selected, ImGuiSelectableFlags_DontClosePopups)) {
				if (ImGui::GetIO().KeyShift) {
					if (is_selected) selectedBoneNames_.erase(pair.first);
					else selectedBoneNames_.insert(pair.first);
				} else {
					selectedBoneNames_.clear();
					selectedBoneNames_.insert(pair.first);
				}
			}
		}
		ImGui::EndCombo();
	}

	static char newBoneName[128] = "";
	ImGui::InputText("新規ボーン名", newBoneName, IM_ARRAYSIZE(newBoneName));
	ImGui::SameLine();
	if (ImGui::Button("ボーン追加")) {
		if (strlen(newBoneName) > 0) {
			customAnimation_.nodeAnimations[newBoneName] = NodeAnimation();
			selectedBoneNames_.clear();
			selectedBoneNames_.insert(newBoneName);
		}
	}

	if (!selectedBoneNames_.empty()) {
		std::string previewName = *selectedBoneNames_.begin();
		if (customAnimation_.nodeAnimations.count(previewName)) {
			NodeAnimation& nodeAnim = customAnimation_.nodeAnimations[previewName];
			ImGui::Text("--- %s %s の編集 ---", previewName.c_str(), selectedBoneNames_.size() > 1 ? "他" : "");

			// Translate
			ImGui::Text("Translate (キー数: %d)", (int)nodeAnim.translate.keyframes.size());
			static float tVal[3] = {0,0,0};
			ImGui::DragFloat3("位置", tVal, 0.01f);
			if (ImGui::Button("現在時刻にTranslateキーを打つ")) {
				KeyframeVector3 kf;
				kf.time = currentFrameTime_;
				kf.value = {tVal[0], tVal[1], tVal[2]};
				nodeAnim.translate.keyframes.push_back(kf);
				// ソートしておく
				std::sort(nodeAnim.translate.keyframes.begin(), nodeAnim.translate.keyframes.end(), [](auto& a, auto& b){ return a.time < b.time; });
			}

			// Rotate
			ImGui::Text("Rotate (キー数: %d)", (int)nodeAnim.rotate.keyframes.size());
			static float rVal[3] = {0,0,0}; // Euler angles for easy editing
			ImGui::DragFloat3("回転 (XYZ度)", rVal, 1.0f);
			if (ImGui::Button("現在時刻にRotateキーを打つ")) {
				KeyframeQuaternion kf;
				kf.time = currentFrameTime_;
				// 簡単なEuler to Quaternion変換
				Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, rVal[0] * 3.141592f / 180.0f);
				Quaternion qYaw   = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, rVal[1] * 3.141592f / 180.0f);
				Quaternion qRoll  = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, rVal[2] * 3.141592f / 180.0f);
				kf.value = MyMath::Normalize(MyMath::Multiply(MyMath::Multiply(qYaw, qPitch), qRoll));
				nodeAnim.rotate.keyframes.push_back(kf);
				std::sort(nodeAnim.rotate.keyframes.begin(), nodeAnim.rotate.keyframes.end(), [](auto& a, auto& b){ return a.time < b.time; });
			}
			
			// Scale
			ImGui::Text("Scale (キー数: %d)", (int)nodeAnim.scale.keyframes.size());
			static float sVal[3] = {1,1,1};
			ImGui::DragFloat3("スケール", sVal, 0.01f);
			if (ImGui::Button("現在時刻にScaleキーを打つ")) {
				KeyframeVector3 kf;
				kf.time = currentFrameTime_;
				kf.value = {sVal[0], sVal[1], sVal[2]};
				nodeAnim.scale.keyframes.push_back(kf);
				std::sort(nodeAnim.scale.keyframes.begin(), nodeAnim.scale.keyframes.end(), [](auto& a, auto& b){ return a.time < b.time; });
			}
		}
	}
#endif
}

void SimulationManager::AddBoneRotationFromDrag(const std::string& boneName, float deltaX, float deltaY) {
    if (boneName.empty()) return;
    
    // まだアニメーションに登録されていないボーンなら自動で登録する
    if (customAnimation_.nodeAnimations.count(boneName) == 0) {
        customAnimation_.nodeAnimations[boneName] = NodeAnimation();
    }
    
    NodeAnimation& nodeAnim = customAnimation_.nodeAnimations[boneName];
    
    Quaternion currentRot = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (!nodeAnim.rotate.keyframes.empty()) {
        currentRot = CalculateValue(nodeAnim.rotate.keyframes, currentFrameTime_);
    }
    
    float speed = 0.01f;
    Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, deltaX * speed);
    Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, deltaY * speed);
    
    Quaternion deltaRot = MyMath::Multiply(qYaw, qPitch);
    currentRot = MyMath::Normalize(MyMath::Multiply(currentRot, deltaRot));
    
    bool found = false;
    for (auto& kf : nodeAnim.rotate.keyframes) {
        if (std::abs(kf.time - currentFrameTime_) < 0.001f) {
            kf.value = currentRot;
            found = true;
            break;
        }
    }
    
    if (!found) {
        KeyframeQuaternion newKf;
        newKf.time = currentFrameTime_;
        newKf.value = currentRot;
        nodeAnim.rotate.keyframes.push_back(newKf);
        std::sort(nodeAnim.rotate.keyframes.begin(), nodeAnim.rotate.keyframes.end(), [](auto& a, auto& b){ return a.time < b.time; });
    }
}

void SimulationManager::AddBoneTranslationFromDrag(const std::string& boneName, float deltaX, float deltaY) {
    if (boneName.empty()) return;
    
    // まだアニメーションに登録されていないボーンなら自動で登録する
    if (customAnimation_.nodeAnimations.count(boneName) == 0) {
        customAnimation_.nodeAnimations[boneName] = NodeAnimation();
    }
    
    NodeAnimation& nodeAnim = customAnimation_.nodeAnimations[boneName];
    
    Vector3 currentTrans = { 0.0f, 0.0f, 0.0f };
    if (!nodeAnim.translate.keyframes.empty()) {
        currentTrans = CalculateValue(nodeAnim.translate.keyframes, currentFrameTime_);
    }
    
    float speed = 0.05f;
    currentTrans.x += deltaX * speed;
    currentTrans.y -= deltaY * speed; // 画面のY軸は下向きなので反転させる
    
    bool found = false;
    for (auto& kf : nodeAnim.translate.keyframes) {
        if (std::abs(kf.time - currentFrameTime_) < 0.001f) {
            kf.value = currentTrans;
            found = true;
            break;
        }
    }
    
    if (!found) {
        KeyframeVector3 newKf;
        newKf.time = currentFrameTime_;
        newKf.value = currentTrans;
        nodeAnim.translate.keyframes.push_back(newKf);
        std::sort(nodeAnim.translate.keyframes.begin(), nodeAnim.translate.keyframes.end(), [](auto& a, auto& b){ return a.time < b.time; });
    }
}


void SimulationManager::UpdateShortcuts() {
	if (!scene_ || !scene_->uiManager_ || scene_->uiManager_->currentSimulationTarget_ != 5) return;

	ImGuiIO& io = ImGui::GetIO();
	
	// ドラッグ中かどうかの判定とUndoへの記録
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		if (!isDragging_ && !selectedBoneNames_.empty()) {
			isDragging_ = true;
			PushUndo(); // ドラッグ開始時に状態を保存
		}
	} else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		if (isDragging_) {
			isDragging_ = false;
		}
	}

	// Space: Play/Pause (UI入力中でない時)
	if (ImGui::IsKeyPressed(ImGuiKey_Space) && !io.WantTextInput) {
		isAnimationPlaying_ = !isAnimationPlaying_;
	}

	// Esc: Deselect all
	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		selectedBoneNames_.clear();
	}

	// Delete/Backspace: Delete current keyframe for selected bones
	if ((ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && !io.WantTextInput) {
		PushUndo();
		DeleteSelectedBonesKeyframes();
	}

	// Ctrl+Z: Undo
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
		Undo();
	}

	// Ctrl+Y: Redo
	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
		Redo();
	}
}

void SimulationManager::PushUndo() {
	undoHistory_.push_back(customAnimation_);
	if (undoHistory_.size() > 50) { // 最大50履歴
		undoHistory_.erase(undoHistory_.begin());
	}
	redoHistory_.clear(); // 新しい操作をしたらRedo履歴は消去
}

void SimulationManager::Undo() {
	if (undoHistory_.empty()) return;
	
	redoHistory_.push_back(customAnimation_);
	customAnimation_ = undoHistory_.back();
	undoHistory_.pop_back();
	
	if (scene_ && scene_->player_) {
		scene_->player_->SetOverrideAnimation(&customAnimation_);
	}
}

void SimulationManager::Redo() {
	if (redoHistory_.empty()) return;
	
	undoHistory_.push_back(customAnimation_);
	customAnimation_ = redoHistory_.back();
	redoHistory_.pop_back();
	
	if (scene_ && scene_->player_) {
		scene_->player_->SetOverrideAnimation(&customAnimation_);
	}
}

void SimulationManager::DeleteSelectedBonesKeyframes() {
	for (const auto& boneName : selectedBoneNames_) {
		if (customAnimation_.nodeAnimations.count(boneName)) {
			auto& keys = customAnimation_.nodeAnimations[boneName].rotate.keyframes;
			for (auto it = keys.begin(); it != keys.end(); ) {
				if (std::abs(it->time - currentFrameTime_) < 0.001f) {
					it = keys.erase(it);
				} else {
					++it;
				}
			}
		}
	}
}
