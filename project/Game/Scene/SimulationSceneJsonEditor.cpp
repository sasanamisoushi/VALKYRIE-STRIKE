#include "SimulationSceneJsonEditor.h"

#include "Game/Player/Player.h"
#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h"
#include "GamePlaySceneHelpers.h"
#include "StageReloadController.h"
#include "engine/Utility/StageLoader.h"

#include <Windows.h>
#include <exception>
#include <fstream>
#include <utility>

bool SimulationSceneJsonEditor::SaveCurrentLayout(
	const std::string& filePath, const SimulationSceneJsonContext& context) {
	auto setStatus = [&context](const std::string& msg) {
		if (context.statusMessage) {
			*context.statusMessage = msg;
		}
	};

	json root;
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) {
			setStatus("scene.json が見つからないため保存できませんでした");
			OutputDebugStringA(("[SimulationSave] File not found: " + filePath + "\n").c_str());
			return false;
		}

		try {
			ifs >> root;
		} catch (const std::exception& e) {
			setStatus("scene.json の読み込みに失敗しました");
			OutputDebugStringA(("[SimulationSave] JSON parse failed: " + std::string(e.what()) + "\n").c_str());
			return false;
		}
	}

	if (!root.contains("objects") || !root["objects"].is_array()) {
		setStatus("scene.json に objects がないため保存できませんでした");
		OutputDebugStringA("[SimulationSave] objects array not found.\n");
		return false;
	}

	json& objects = root["objects"];
	size_t playerObjectIndex = kInvalidSceneObjectIndex;
	std::vector<size_t> enemyObjectIndices;
	std::vector<size_t> obstacleObjectIndices;

	for (size_t index = 0; index < objects.size(); ++index) {
		const json& objectData = objects[index];
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

	if (context.player && playerObjectIndex != kInvalidSceneObjectIndex) {
		const Vector3 playerRotation = EulerFromQuaternionXYZ(context.player->GetQuaternion());
		WriteSceneTransform(objects[playerObjectIndex], context.player->GetPosition(), &playerRotation, nullptr);
		++savedCount;
	}

	if (context.obstacles) {
		size_t obstacleIndex = 0;
		for (const auto& obstacle : *context.obstacles) {
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
	}

	if (context.enemies && context.enemySpawns) {
		for (const auto& enemy : *context.enemies) {
			if (!enemy || enemy->IsDead()) {
				continue;
			}

			const Vector3 enemyPosition = enemy->GetPosition();
			const Vector3 enemyRotation = enemy->GetRotation();
			const size_t spawnPointIndex = enemy->GetSpawnPointIndex();

			if (spawnPointIndex != Enemy::kNoSpawnPoint && spawnPointIndex < enemyObjectIndices.size()) {
				WriteSceneTransform(objects[enemyObjectIndices[spawnPointIndex]], enemyPosition, &enemyRotation, nullptr);
				if (spawnPointIndex < context.enemySpawns->size()) {
					(*context.enemySpawns)[spawnPointIndex].position = enemyPosition;
					(*context.enemySpawns)[spawnPointIndex].rotation = enemyRotation;
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
			context.enemySpawns->push_back(spawnData);
			enemy->SetSpawnPointIndex(context.enemySpawns->size() - 1);
			enemyObjectIndices.push_back(objects.size() - 1);
			++savedCount;
		}

		// 増援・ボスなど、現在は未出現の敵もエディタで動かした位置を保存する。
		for (size_t spawnPointIndex = 0;
			spawnPointIndex < context.enemySpawns->size() && spawnPointIndex < enemyObjectIndices.size();
			++spawnPointIndex) {
			const EnemySpawnData& spawn = (*context.enemySpawns)[spawnPointIndex];
			WriteSceneTransform(objects[enemyObjectIndices[spawnPointIndex]], spawn.position, &spawn.rotation, nullptr);
			++savedCount;
		}
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		setStatus("scene.json を書き込めませんでした");
		OutputDebugStringA(("[SimulationSave] Failed to open for write: " + filePath + "\n").c_str());
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	if (context.stageReloadController) {
		context.stageReloadController->SyncWriteTime(filePath);
	}

	setStatus("現在の配置を scene.json に保存しました。実ゲームにも反映されます");
	OutputDebugStringA(("[SimulationSave] Saved " + std::to_string(savedCount) + " transforms.\n").c_str());
	return true;
}

bool SimulationSceneJsonEditor::AddEnemySpawn(
	const std::string& filePath,
	const std::string& name,
	const std::string& enemyType,
	const Vector3& position,
	const Vector3& rotation,
	const SimulationSceneJsonContext& context) {
	auto setStatus = [&context](const std::string& msg) {
		if (context.statusMessage) {
			*context.statusMessage = msg;
		}
	};

	json root;
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) {
			setStatus("scene.json が見つからないため敵を追加できませんでした");
			return false;
		}

		try {
			ifs >> root;
		} catch (const std::exception&) {
			setStatus("scene.json の読み込みに失敗したため敵を追加できませんでした");
			return false;
		}
	}

	if (!root.contains("objects") || !root["objects"].is_array()) {
		setStatus("scene.json に objects がないため敵を追加できませんでした");
		return false;
	}

	json& objects = root["objects"];
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
		setStatus("scene.json を書き込めませんでした");
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	if (context.stageReloadController) {
		context.stageReloadController->SyncWriteTime(filePath);
	}

	// ファイル保存だけでなく、ゲーム中のプレビューにも即時反映する。
	if (context.reloadSceneJson) {
		context.reloadSceneJson();
	}
	setStatus("敵配置を scene.json に保存し、プレビューへ反映しました");
	return true;
}

bool SimulationSceneJsonEditor::MoveEnemySpawn(
	const std::string& filePath,
	size_t sourceIndex,
	size_t targetIndex,
	const SimulationSceneJsonContext& context) {
	auto setStatus = [&context](const std::string& msg) {
		if (context.statusMessage) {
			*context.statusMessage = msg;
		}
	};

	json root;
	{
		std::ifstream ifs(filePath);
		if (!ifs.is_open()) {
			setStatus("scene.json が見つからないためスポーン順を変更できませんでした");
			return false;
		}

		try {
			ifs >> root;
		} catch (const std::exception&) {
			setStatus("scene.json の読み込みに失敗したためスポーン順を変更できませんでした");
			return false;
		}
	}

	if (!root.contains("objects") || !root["objects"].is_array()) {
		setStatus("scene.json に objects がないためスポーン順を変更できませんでした");
		return false;
	}

	json& objects = root["objects"];
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
		setStatus("scene.json を書き込めませんでした");
		return false;
	}
	ofs << root.dump(4);
	ofs.close();

	if (context.stageReloadController) {
		context.stageReloadController->SyncWriteTime(filePath);
	}

	if (context.reloadSceneJson) {
		context.reloadSceneJson();
	}
	setStatus("敵の初期スポーン順を変更し、プレビューへ反映しました");
	return true;
}
