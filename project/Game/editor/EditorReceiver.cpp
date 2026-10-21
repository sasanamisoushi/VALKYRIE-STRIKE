#define WIN32_LEAN_AND_MEAN // ← これが「古い辞書を呼ばない」魔法の呪文です
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <exception>
#include <unordered_map>

#include "EditorReceiver.h"
#include "3D/ModelManager.h"
#include "engine/Utility/StageValidation.h"
#include "externals/json.hpp"
#include "Game/enemy/Enemy.h"
#include "Game/Player/Player.h"
#include "Game/obstacle/Obstacle.h"


// Windowsの通信用ライブラリを自動でリンクする
#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;

namespace {

constexpr int kReceiveBufferSize = 65536;

std::wstring Utf8ToWide(const std::string& text) {
	if (text.empty()) {
		return {};
	}

	int wideSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, nullptr, 0);
	if (wideSize <= 0) {
		return std::wstring(text.begin(), text.end());
	}

	std::wstring wideText(static_cast<size_t>(wideSize), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, wideText.data(), wideSize);
	if (!wideText.empty() && wideText.back() == L'\0') {
		wideText.pop_back();
	}
	return wideText;
}

Vector3 ToGamePosition(const json &point) {
	return {
		point[0].get<float>(),
		point[2].get<float>(),
		point[1].get<float>()
	};
}

std::unordered_map<std::string, EnemyFlightPath> LoadFlightPaths(const json &root) {
	std::unordered_map<std::string, EnemyFlightPath> paths;

	if (!root.contains("paths") || !root["paths"].is_array()) {
		return paths;
	}

	for (const auto &pathData : root["paths"]) {
		if (!pathData.is_object() || !pathData.contains("id") || !pathData["id"].is_string()) {
			continue;
		}

		EnemyFlightPath path;
		path.loop = pathData.value("loop", false);
		path.speed = pathData.value("speed", 0.05f);

		if (pathData.contains("points") && pathData["points"].is_array()) {
			for (const auto &point : pathData["points"]) {
				if (point.is_array() && point.size() == 3) {
					path.points.push_back(ToGamePosition(point));
				}
			}
		}

		paths[pathData["id"].get<std::string>()] = std::move(path);
	}

	return paths;
}

std::vector<std::string> SplitReinforcementTriggers(const std::string &text) {
	std::vector<std::string> result;
	size_t start = 0;
	while (start < text.size()) {
		const size_t end = text.find(',', start);
		std::string name = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
		const size_t first = name.find_first_not_of(" \t");
		if (first != std::string::npos) {
			const size_t last = name.find_last_not_of(" \t");
			result.push_back(name.substr(first, last - first + 1));
		}
		if (end == std::string::npos) {
			break;
		}
		start = end + 1;
	}
	return result;
}

EnemySpawnData BuildEnemySpawnData(const json &objData, const Vector3 &position, const Vector3 &rotation, const std::unordered_map<std::string, EnemyFlightPath> &paths) {
	EnemySpawnData spawnData;
	spawnData.name = objData.value("name", "UnknownEnemy");
	spawnData.position = position;
	spawnData.rotation = rotation;
	std::string typeStr = "";
	if (objData.contains("enemy") && objData["enemy"].is_object() && objData["enemy"].contains("type") && objData["enemy"]["type"].is_string()) {
		typeStr = objData["enemy"]["type"].get<std::string>();
	} else if (objData.contains("enemy_type") && objData["enemy_type"].is_string()) {
		typeStr = objData["enemy_type"].get<std::string>();
	}

	std::string upperType = typeStr;
	for (char &c : upperType) { c = static_cast<char>(toupper(static_cast<unsigned char>(c))); }
	std::string upperName = spawnData.name;
	for (char &c : upperName) { c = static_cast<char>(toupper(static_cast<unsigned char>(c))); }

	StageLoader::DetermineEnemyTypeFlags(typeStr, spawnData.name, spawnData.isGround, spawnData.isJammer, spawnData.isBoss);

	bool hasExplicitInitialSpawnSetting = false;
	bool explicitInitialSpawnValue = true;

	auto parseInitialSpawnFlag = [&](const json &container) {
		if (container.contains("is_initial_spawn") && container["is_initial_spawn"].is_boolean()) {
			hasExplicitInitialSpawnSetting = true;
			explicitInitialSpawnValue = container["is_initial_spawn"].get<bool>();
		} else if (container.contains("isInitialSpawn") && container["isInitialSpawn"].is_boolean()) {
			hasExplicitInitialSpawnSetting = true;
			explicitInitialSpawnValue = container["isInitialSpawn"].get<bool>();
		}
	};

	parseInitialSpawnFlag(objData);

	if (objData.contains("enemy") && objData["enemy"].is_object()) {
		parseInitialSpawnFlag(objData["enemy"]);
	}

	if (objData.contains("path_id") && objData["path_id"].is_string()) {
		auto pathIt = paths.find(objData["path_id"].get<std::string>());
		if (pathIt != paths.end()) {
			spawnData.flightPath = pathIt->second;
		}
	}

	if (objData.contains("reinforcement") && objData["reinforcement"].is_object()) {
		const auto &reinforcement = objData["reinforcement"];
		parseInitialSpawnFlag(reinforcement);
		spawnData.reinforcementTriggerName = reinforcement.value("trigger", "");
		spawnData.reinforcementTriggerNames = SplitReinforcementTriggers(spawnData.reinforcementTriggerName);
		spawnData.remainingReinforcementTriggers = spawnData.reinforcementTriggerNames;
		spawnData.reinforcementDelayFrames = reinforcement.value("delay", 0);
		if (spawnData.reinforcementDelayFrames < 0) {
			spawnData.reinforcementDelayFrames = 0;
		}
	}

	if (spawnData.name.find("Boss") == 0) {
		spawnData.isInitialSpawn = false;
	} else if (hasExplicitInitialSpawnSetting) {
		spawnData.isInitialSpawn = explicitInitialSpawnValue;
	} else {
		spawnData.isInitialSpawn = !spawnData.HasReinforcementTrigger();
	}

	return spawnData;
}

std::string GetObjectBaseName(const json& objData) {
	if (!objData.contains("name") || !objData["name"].is_string()) {
		return "ObstacleBox";
	}

	std::string objName = objData["name"].get<std::string>();
	size_t dotPos = objName.find('.');
	return (dotPos != std::string::npos) ? objName.substr(0, dotPos) : objName;
}

bool ResourceFileExists(const std::string& fileName) {
	std::string filePath = "resources/" + fileName;
	std::wstring widePath = Utf8ToWide(filePath);
	DWORD attributes = GetFileAttributesW(widePath.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool TryLoadModelFile(const std::string& modelFile) {
	if (modelFile.empty()) {
		return false;
	}

	ModelManager* modelManager = ModelManager::GetInstance();
	if (modelManager->FindModel(modelFile) != nullptr) {
		return true;
	}

	if (!ResourceFileExists(modelFile)) {
		OutputDebugStringA((" [EditorReceiver] Model file not found: resources/" + modelFile + "\n").c_str());
		return false;
	}

	modelManager->LoadModel(modelFile);
	return modelManager->FindModel(modelFile) != nullptr;
}

std::string ResolveObstacleModelName(const json& objData) {
	std::string objectName = GetObjectBaseName(objData);
	if (objectName.find("StageBounds") != std::string::npos) {
		return objectName;
	}

	if (objData.contains("model") && objData["model"].is_string()) {
		std::string modelFile = objData["model"].get<std::string>();
		if (TryLoadModelFile(modelFile)) {
			return modelFile;
		}
	}

	return objectName;
}

}

EditorReceiver *EditorReceiver::GetInstance() {
	static EditorReceiver instance;
	return &instance;
}

void EditorReceiver::Initialize() {
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        // 新しいGameplayでは、現在のBlenderデータを同じ内容でも再適用する。
        latestJsonData_.clear();
        lastAppliedJsonData_.clear();
        hasNewData_ = false;
    }

    isRunning_ = true;
    receiveThread_ = std::thread(&EditorReceiver::ReceiveLoop, this); // 裏で受信スタート！
}

void EditorReceiver::Finalize() {
	isRunning_ = false;
    if (receiveThread_.joinable()) {
        receiveThread_.join(); // スレッドを安全に終了させる
    }

    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        latestJsonData_.clear();
        hasNewData_ = false;
    }
}

void EditorReceiver::ReceiveLoop() {
	// Windowsネットワークの初期化
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

	// UDPソケット（受け口）を作る
	SOCKET recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	sockaddr_in recvAddr;
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(50000); // 💡ポート50000番を専用の通り道にする
	// Blender は同じPCの 127.0.0.1 にだけ送信する。全ネットワークで
	// 待ち受けないことで、外部から受信せずファイアウォール確認も不要にする。
	recvAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	bind(recvSocket, (SOCKADDR *)&recvAddr, sizeof(recvAddr));

	// 通信が来ていなくても処理が止まらないようにする（ノンブロッキング）
	u_long val = 1;
	ioctlsocket(recvSocket, FIONBIO, &val);

	char buffer[kReceiveBufferSize];

	while (isRunning_) {
		sockaddr_in senderAddr;
		int senderAddrSize = sizeof(senderAddr);
		int bytesRecv = recvfrom(recvSocket, buffer, sizeof(buffer) - 1, 0, (SOCKADDR *)&senderAddr, &senderAddrSize);

		if (bytesRecv > 0) {
			buffer[bytesRecv] = '\0'; // 文字列の終わりにフタをする

			// 受け取ったJSONを安全に保存！
			std::lock_guard<std::mutex> lock(dataMutex_);
			latestJsonData_ = buffer;
			hasNewData_ = true;
		} else {
			Sleep(10); // データがない場合は少し休む
		}
	}

	closesocket(recvSocket);
	WSACleanup();
}

bool EditorReceiver::Update(Player *player, std::list<std::unique_ptr<Enemy>> &enemies, std::list<std::unique_ptr<Obstacle>> &obstacles, std::vector<EnemySpawnData> &enemySpawns) {
	std::string jsonStr;
	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		if (!hasNewData_) return false; // 💡新しいデータが無ければ何もしない（一瞬で終わる）
		jsonStr = latestJsonData_;
		hasNewData_ = false;
	}

	if (jsonStr == lastAppliedJsonData_) {
		return false;
	}

	// 新しいデータが届いていたら、ゲーム内の敵を一気に書き換える！
	try {
		json root = json::parse(jsonStr);
#ifdef CG2_ENABLE_STAGE_VALIDATION
		const StageValidation::Report &validationReport = StageValidation::ValidateSceneJson(root, "Blender realtime data");
		if (validationReport.HasErrors()) {
			return false;
		}
#endif
		if (root.contains("objects") && root["objects"].is_array()) {
			std::unordered_map<std::string, EnemyFlightPath> flightPaths = LoadFlightPaths(root);

			enemies.clear(); // 一旦リセットして再配置（エディタモード用のシンプルな同期）
			obstacles.clear();
			enemySpawns.clear();

			for (auto &objData : root["objects"]) {
				// ==========================================
				// 1. 座標・スケール・回転 の取得
				// ==========================================
				auto &trans = objData["transform"]["translation"];
				Vector3 position = { trans[0].get<float>(), trans[2].get<float>(), trans[1].get<float>() };

				auto &scaleData = objData["transform"]["scale"];
				Vector3 scale = { scaleData[0].get<float>(), scaleData[2].get<float>(), scaleData[1].get<float>() };

				// ★追加：回転データを Degree(度) から Radian(弧度) に変換して読み込む！
				Vector3 rotation = { 0.0f, 0.0f, 0.0f };
				if (objData["transform"].contains("rotation")) {
					auto &rotData = objData["transform"]["rotation"];
					float toRad = 3.141592654f / 180.0f; // 変換用の魔法の数字
					rotation = {
						rotData[0].get<float>() * toRad,
						rotData[2].get<float>() * toRad,
						rotData[1].get<float>() * toRad
					};
				}

				// ==========================================
				// 2. 種類(category)ごとの生成処理
				// ==========================================
				std::string name = objData.value("name", "");
				std::string category = "";
				if (objData.contains("category") && objData["category"].is_string()) {
					category = objData["category"].get<std::string>();
				} else if (objData.contains("game_obj_type") && objData["game_obj_type"].is_string()) {
					category = objData["game_obj_type"].get<std::string>();
				}

				std::string upperName = name;
				for (char &c : upperName) { c = static_cast<char>(toupper(static_cast<unsigned char>(c))); }

				if (category.empty() || category == "NONE") {
					if (upperName.find("PLAYER") != std::string::npos) {
						category = "PLAYER";
					} else if (upperName.find("ENEMY") != std::string::npos || upperName.find("VF3") != std::string::npos || upperName.find("VF1") != std::string::npos || upperName.find("BOSS") != std::string::npos) {
						category = "ENEMY";
					} else if (upperName.find("TERRAIN") != std::string::npos || upperName.find("STAGE") != std::string::npos || upperName.find("OBSTACLE") != std::string::npos || upperName.find("BLOCK") != std::string::npos || upperName.find("OBJECT") != std::string::npos) {
						category = "OBSTACLE";
					}
				}

				if (category == "PLAYER") {
					if (player) {
						player->SetPosition(position);
						player->SetRotation(rotation);
					}
				} else if (category == "ENEMY") {
					enemySpawns.push_back(BuildEnemySpawnData(objData, position, rotation, flightPaths));
				} else if (category == "OBSTACLE") {
					std::string modelName = ResolveObstacleModelName(objData);
					auto newObstacle = std::make_unique<Obstacle>();
					newObstacle->Initialize(modelName, position, rotation, scale);

					// 通常の StageLoader と同じコライダー設定をライブ更新でも適用する。
					if (objData.contains("useMeshCollider") && objData["useMeshCollider"].is_boolean()) {
						newObstacle->SetUseMeshCollider(objData["useMeshCollider"].get<bool>());
					}
					if (objData.contains("isCollisionEnabled") && objData["isCollisionEnabled"].is_boolean()) {
						newObstacle->SetCollisionEnabled(objData["isCollisionEnabled"].get<bool>());
					}
					if (objData.contains("collisionOffset") && objData["collisionOffset"].is_array()) {
						const auto &arr = objData["collisionOffset"];
						if (arr.size() == 3) {
							newObstacle->SetCollisionOffset({ arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>() });
						}
					}
					if (objData.contains("collisionScale") && objData["collisionScale"].is_array()) {
						const auto &arr = objData["collisionScale"];
						if (arr.size() == 3) {
							newObstacle->SetCollisionScale({ arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>() });
						}
					}

					const std::string objectName = GetObjectBaseName(objData);
					if (objectName.find("Terrain") != std::string::npos || objectName.find("terrain") != std::string::npos) {
						newObstacle->SetUseMeshCollider(true);
					}
					if (modelName.find("StageBounds") != std::string::npos) {
						newObstacle->SetStageBounds(true);
					}
					// このフレームから接地判定できるようワールド三角形を即時生成する。
					newObstacle->Update();
					obstacles.push_back(std::move(newObstacle));
				}
				// 互換性用（Blenderで種類を設定し忘れた時のための予備）
				else if (objData.contains("enemy")) {
					enemySpawns.push_back(BuildEnemySpawnData(objData, position, rotation, flightPaths));
				}
				else if (objData.contains("type") && objData["type"].get<std::string>() == "MESH") {
					std::string modelName = ResolveObstacleModelName(objData);

					auto newObstacle = std::make_unique<Obstacle>();
					newObstacle->Initialize(modelName, position, rotation, scale);
					const std::string objectName = GetObjectBaseName(objData);
					if (objectName.find("Terrain") != std::string::npos || objectName.find("terrain") != std::string::npos) {
						newObstacle->SetUseMeshCollider(true);
					}
					newObstacle->Update();
					obstacles.push_back(std::move(newObstacle));
				}
			}
			lastAppliedJsonData_ = jsonStr;
			return true;
		}
	} catch (const std::exception &e) {
#ifdef CG2_ENABLE_STAGE_VALIDATION
		StageValidation::SetErrorReport("Blender realtime data", "JSON の解析に失敗しました: " + std::string(e.what()));
#else
		(void)e;
#endif
		// もし通信が途切れたり変なデータが来ても無視してゲームを落とさない
	}
	return false;
}
