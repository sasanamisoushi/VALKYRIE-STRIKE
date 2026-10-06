#pragma once
#include "engine/Debug/ImGuiManager.h"
#include <externals/imgui/imgui.h>
#include "engine/math/MyMath.h"
#include "externals/json.hpp"
#include "Game/Player/Player.h"

using json = nlohmann::json;

namespace {
	constexpr int kEnemyRespawnDelayFrames = 180;
	constexpr int kNoRespawnTimer = -1;
	constexpr float kNormalCameraFovY = 0.45f;
	constexpr float kNormalCameraFarClip = 220.0f;
	constexpr float kCinematicCameraFovY = 0.70f;
	constexpr float kCinematicCameraFarClip = 250.0f;
	constexpr float kCinematicCameraFocusBlend = 0.18f;
	constexpr float kCinematicCameraDirectionBlend = 0.10f;
	constexpr float kCinematicCameraPositionBlend = 0.10f;
	constexpr float kCinematicCameraRotationBlend = 0.12f;
	constexpr float kAimAssistScreenRadius = 96.0f;
	constexpr float kAimAssistMaxDistance = 180.0f;
	constexpr size_t kMultiLockMaxTargets = 12;
	constexpr int kMultiLockAcquireIntervalFrames = 2;
	constexpr float kMultiLockScreenRadius = 320.0f;
	constexpr float kMultiLockMaxDistance = 240.0f;
	constexpr float kRadiansToDegrees = 180.0f / 3.141592654f;
	constexpr size_t kInvalidSceneObjectIndex = static_cast<size_t>(-1);
	const char *kSimulationActionsFilePath = "resources/simulation_actions.json";
	const char *kMissilePresetsFilePath = "resources/missile_presets.json";
	const char *kPlayerModelName = "vf-15c/scene.gltf";
	const char *kLockOnReticleTexturePath = "resources/lock_on_reticle.png";
	const char *kMissileLockOnReticleTexturePath = "resources/missile_lock_on_reticle.png";
	const char *kAimCursorTexturePath = "resources/aim_cursor.png";
	const char *kBoundaryAlertTexturePath = "resources/boundary_alert.png";

	Vector3 SubtractVector3(const Vector3 &lhs, const Vector3 &rhs) {
		return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
	}

	Vector3 AddVector3(const Vector3 &lhs, const Vector3 &rhs) {
		return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
	}

	Vector3 ScaleVector3(const Vector3 &value, float scale) {
		return { value.x * scale, value.y * scale, value.z * scale };
	}

	Vector3 FlattenYVector3(const Vector3 &value) {
		return { value.x, 0.0f, value.z };
	}

	std::filesystem::path GetCurrentExecutablePath() {
		wchar_t modulePath[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (length == 0) {
			return {};
		}
		return std::filesystem::path(modulePath);
	}

	bool LaunchSimulationExecutable() {
		const std::filesystem::path currentExe = GetCurrentExecutablePath();
		if (currentExe.empty()) {
			return false;
		}

		const std::filesystem::path simulationExe = currentExe.parent_path() / L"ValkyrieStrikeSimulation.exe";
		const std::filesystem::path launchExe = std::filesystem::exists(simulationExe) ? simulationExe : currentExe;
		const wchar_t *parameters = (launchExe == currentExe) ? L"--simulation" : nullptr;
		const std::filesystem::path workDir = std::filesystem::current_path();

		HINSTANCE result = ShellExecuteW(
			nullptr,
			L"open",
			launchExe.c_str(),
			parameters,
			workDir.c_str(),
			SW_SHOWNORMAL);
		return reinterpret_cast<intptr_t>(result) > 32;
	}

	std::string GetJsonString(const json &objectData, const char *key) {
		if (!objectData.contains(key) || !objectData[key].is_string()) {
			return {};
		}
		return objectData[key].get<std::string>();
	}

	bool IsSceneCategory(const json &objectData, const char *category) {
		return GetJsonString(objectData, "category") == category;
	}

	bool IsSceneEnemyObject(const json &objectData) {
		return IsSceneCategory(objectData, "ENEMY") || objectData.contains("enemy");
	}

	bool IsScenePlayerObject(const json &objectData) {
		return IsSceneCategory(objectData, "PLAYER");
	}

	bool IsSceneObstacleObject(const json &objectData) {
		if (IsScenePlayerObject(objectData) || IsSceneEnemyObject(objectData)) {
			return false;
		}
		if (IsSceneCategory(objectData, "OBSTACLE")) {
			return true;
		}
		if (GetJsonString(objectData, "type") == "MESH") {
			std::string name = GetJsonString(objectData, "name");
			if (name.find("Terrain") != std::string::npos || name.find("terrain") != std::string::npos) {
				return true;
			}
		}
		return false;
	}

	json ToBlenderPositionJson(const Vector3 &position) {
		return json::array({ position.x, position.z, position.y });
	}

	json ToBlenderRotationJson(const Vector3 &rotation) {
		return json::array({
			rotation.x * kRadiansToDegrees,
			rotation.z * kRadiansToDegrees,
			rotation.y * kRadiansToDegrees
		});
	}

	json ToBlenderScaleJson(const Vector3 &scale) {
		return json::array({ scale.x, scale.z, scale.y });
	}

	void WriteSceneTransform(
		json &objectData,
		const Vector3 &position,
		const Vector3 *rotation,
		const Vector3 *scale) {

		if (!objectData.contains("transform") || !objectData["transform"].is_object()) {
			objectData["transform"] = json::object();
		}

		json &transform = objectData["transform"];
		transform["translation"] = ToBlenderPositionJson(position);
		if (rotation) {
			transform["rotation"] = ToBlenderRotationJson(*rotation);
		}
		if (scale) {
			transform["scale"] = ToBlenderScaleJson(*scale);
		}
	}

	Vector3 EulerFromQuaternionXYZ(const Quaternion &quaternion) {
		const Matrix4x4 matrix = MyMath::MakeRotateMatrix(MyMath::Normalize(quaternion));
		const float sinY = std::clamp(-matrix.m[0][2], -1.0f, 1.0f);
		const float y = std::asin(sinY);
		const float cosY = std::cos(y);

		Vector3 rotation = { 0.0f, y, 0.0f };
		if (std::abs(cosY) > 0.0001f) {
			rotation.x = std::atan2(matrix.m[1][2], matrix.m[2][2]);
			rotation.z = std::atan2(matrix.m[0][1], matrix.m[0][0]);
		} else {
			rotation.z = std::atan2(-matrix.m[1][0], matrix.m[1][1]);
		}
		return rotation;
	}

	bool SceneObjectNameExists(const json &objects, const std::string &name) {
		for (const auto &objectData : objects) {
			if (GetJsonString(objectData, "name") == name) {
				return true;
			}
		}
		return false;
	}

	std::string MakeUniqueSceneObjectName(const json &objects, const std::string &prefix) {
		for (int index = 1; index < 10000; ++index) {
			std::string name = prefix + std::to_string(index);
			if (!SceneObjectNameExists(objects, name)) {
				return name;
			}
		}
		return prefix + "9999";
	}

	std::string TrimActionName(const std::string &name) {
		size_t begin = 0;
		while (begin < name.size() && std::isspace(static_cast<unsigned char>(name[begin]))) {
			++begin;
		}

		size_t end = name.size();
		while (end > begin && std::isspace(static_cast<unsigned char>(name[end - 1]))) {
			--end;
		}

		return name.substr(begin, end - begin);
	}

	json ToVector3Json(const Vector3 &value) {
		return json::array({ value.x, value.y, value.z });
	}

	Vector3 ReadVector3Json(const json &value, const Vector3 &fallback) {
		if (!value.is_array() || value.size() < 3) {
			return fallback;
		}

		return {
			value[0].get<float>(),
			value[1].get<float>(),
			value[2].get<float>()
		};
	}

	float ReadJsonFloat(const json &objectData, const char *key, float fallback) {
		if (!objectData.is_object() || !objectData.contains(key) || !objectData[key].is_number()) {
			return fallback;
		}
		return objectData[key].get<float>();
	}

	int ReadJsonInt(const json &objectData, const char *key, int fallback) {
		if (!objectData.is_object() || !objectData.contains(key) || !objectData[key].is_number_integer()) {
			return fallback;
		}
		return objectData[key].get<int>();
	}

	json PlayerModeParamsToJson(const PlayerModeParams &params) {
		json data;
		data["maxMoveSpeed"] = params.maxMoveSpeed;
		data["moveAcceleration"] = params.moveAcceleration;
		data["moveDamping"] = params.moveDamping;
		data["pitchSpeed"] = params.pitchSpeed;
		data["yawSpeed"] = params.yawSpeed;
		data["rollSpeed"] = params.rollSpeed;
		return data;
	}

	void ApplyPlayerModeParamsFromJson(const json &data, PlayerModeParams &params) {
		params.maxMoveSpeed = ReadJsonFloat(data, "maxMoveSpeed", params.maxMoveSpeed);
		params.moveAcceleration = ReadJsonFloat(data, "moveAcceleration", params.moveAcceleration);
		params.moveDamping = ReadJsonFloat(data, "moveDamping", params.moveDamping);
		params.pitchSpeed = ReadJsonFloat(data, "pitchSpeed", params.pitchSpeed);
		params.yawSpeed = ReadJsonFloat(data, "yawSpeed", params.yawSpeed);
		params.rollSpeed = ReadJsonFloat(data, "rollSpeed", params.rollSpeed);
	}

	float LengthSqVector3(const Vector3 &value) {
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}

	float LengthVector3(const Vector3 &value) {
		return std::sqrt(LengthSqVector3(value));
	}

	Vector3 NormalizeOrVector3(const Vector3 &value, const Vector3 &fallback) {
		const float length = LengthVector3(value);
		if (length <= 0.0001f) {
			return fallback;
		}
		return ScaleVector3(value, 1.0f / length);
	}

	Vector3 LerpVector3(const Vector3 &from, const Vector3 &to, float t) {
		return {
			from.x + (to.x - from.x) * t,
			from.y + (to.y - from.y) * t,
			from.z + (to.z - from.z) * t
		};
	}

	Quaternion MakeLookQuaternion(const Vector3 &forward) {
		const Vector3 normalizedForward = NormalizeOrVector3(forward, { 0.0f, 0.0f, 1.0f });
		const float clampedY = std::clamp(normalizedForward.y, -1.0f, 1.0f);
		// このエンジンではローカル +Z が正面かつ、正の pitch は上向きになる。
		// そのため、見たい方向の Y 成分と同じ符号で pitch を作る。
		const float pitch = std::asin(clampedY);
		const float yaw = std::atan2(normalizedForward.x, normalizedForward.z);

		Quaternion qPitch = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, pitch);
		Quaternion qYaw = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);
		return MyMath::Normalize(MyMath::Multiply(qYaw, qPitch));
	}

	bool IsImGuiKeyboardCaptureActive() {
#ifdef ENABLE_IMGUI
		if (!ImGuiManager::IsVisible() || ImGui::GetCurrentContext() == nullptr) {
			return false;
		}

		const ImGuiIO &io = ImGui::GetIO();
		return io.WantCaptureKeyboard || io.WantTextInput || ImGui::IsAnyItemActive();
#else
		return false;
#endif
	}

	bool IsImGuiMouseCaptureActive() {
#ifdef ENABLE_IMGUI
		if (!ImGuiManager::IsVisible() || ImGui::GetCurrentContext() == nullptr) {
			return false;
		}

		const ImGuiIO &io = ImGui::GetIO();
		// Game View 上の Image は ImGui の項目だが、ゲームへの入力対象でもある。
		// ここをUI操作扱いにすると、ゲーム画面をクリックしても射撃できなくなる。
		Vector2 gameViewMousePosition;
		if (FlyCamera::GetGameViewMousePos(io.MousePos.x, io.MousePos.y, gameViewMousePosition)) {
			return false;
		}
		return io.WantCaptureMouse || ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();
#else
		return false;
#endif
	}

	bool GetOverlayBounds(float &minX, float &minY, float &maxX, float &maxY) {
		if (FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY) && maxX > minX && maxY > minY) {
			return true;
		}

		minX = 0.0f;
		minY = 0.0f;
		maxX = static_cast<float>(WinApp::GetClientWidth());
		maxY = static_cast<float>(WinApp::GetClientHeight());
		return maxX > minX && maxY > minY;
	}

	void DrawLockOnOverlaySprite(const Enemy *target, const Matrix4x4 &viewProjectionMatrix, Sprite* lockOnReticleSprite_, bool isJammed = false, bool isFighterLockDanger = false) {
		if (!target) {
			return;
		}
		
		try {
			if (target->IsDead()) {
				return;
			}
		} catch(...) {
			return;
		}

		Vector3 worldPosition = target->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = target->GetCollisionRadius();
		} catch(...) {}
		worldPosition.y += collisionRadius * 0.3f;
		float minX = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
			return;
		}

		const float width = maxX - minX;
		const float height = maxY - minY;

		Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
		if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) {
			return;
		}
		if (screenPosition.x < 0.0f || screenPosition.x > width ||
			screenPosition.y < 0.0f || screenPosition.y > height) {
			return;
		}

		float winAppWidth = static_cast<float>(WinApp::GetClientWidth());
		float winAppHeight = static_cast<float>(WinApp::GetClientHeight());

		float spriteX = screenPosition.x * winAppWidth / width;
		float spriteY = screenPosition.y * winAppHeight / height;
		if (isJammed) {
			spriteX += (rand() % 11 - 5) * 1.5f;
			spriteY += (rand() % 11 - 5) * 1.5f;
		}
		const Vector2 center = { spriteX, spriteY };

		const float reticleSize = std::clamp(76.0f + collisionRadius * 12.0f, 76.0f, 116.0f);
		const float aspectScaleX = (height / width) * (winAppWidth / winAppHeight);

		if (lockOnReticleSprite_) {
			lockOnReticleSprite_->SetPosition(center);
			lockOnReticleSprite_->SetSize({ reticleSize * aspectScaleX, reticleSize });
			lockOnReticleSprite_->SetColor(isFighterLockDanger
				? Vector4{ 1.0f, 0.18f, 0.12f, 1.0f }
				: Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
			lockOnReticleSprite_->Update();
			lockOnReticleSprite_->Draw();
		}
	}

	void DrawIndividualMissileLockOnOverlaySprite(const Enemy *target, const Matrix4x4 &viewProjectionMatrix, Sprite* missileLockOnReticleSprite_, bool isJammed, int lockIndex, int totalLockCount) {
		if (!target) {
			return;
		}

		try {
			if (target->IsDead()) {
				return;
			}
		} catch (...) {
			return;
		}

		Vector3 worldPosition = target->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = target->GetCollisionRadius();
		} catch (...) {}
		worldPosition.y += collisionRadius * 0.3f;

		float minX = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
			return;
		}

		const float width = maxX - minX;
		const float height = maxY - minY;
		Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
		if (screenPosition.z < 0.0f) {
			return;
		}
		screenPosition.x = std::clamp(screenPosition.x, 20.0f, width - 20.0f);
		screenPosition.y = std::clamp(screenPosition.y, 20.0f, height - 20.0f);

		const float winAppWidth = static_cast<float>(WinApp::GetClientWidth());
		const float winAppHeight = static_cast<float>(WinApp::GetClientHeight());
		float spriteX = screenPosition.x * winAppWidth / width;
		float spriteY = screenPosition.y * winAppHeight / height;

		if (isJammed) {
			spriteX += (rand() % 11 - 5) * 1.5f;
			spriteY += (rand() % 11 - 5) * 1.5f;
		}

		if (missileLockOnReticleSprite_) {
			const float baseReticleSize = std::clamp(32.0f + collisionRadius * 4.0f, 28.0f, 56.0f);
			const float aspectScaleX = (height / width) * (winAppWidth / winAppHeight);

			static float globalTimeAngle = 0.0f;
			globalTimeAngle += 0.025f;

			// 近くの敵や同一敵への重複時も各弾が視認できるよう、全弾数(totalLockCount)と弾インデックス(lockIndex)に応じたオフセットを計算
			int effectiveTotal = (std::max)(totalLockCount, 1);
			float offsetDistance = (totalLockCount > 1) ? std::clamp(28.0f + collisionRadius * 6.0f, 24.0f, 48.0f) : 0.0f;
			float angle = (3.14159265f * 2.0f / static_cast<float>(effectiveTotal)) * static_cast<float>(lockIndex - 1) + globalTimeAngle;

			float finalX = spriteX + std::cos(angle) * offsetDistance;
			float finalY = spriteY + std::sin(angle) * offsetDistance;

			// 誘導弾のターゲットマーカーはすべて統一して鮮やかな赤色にする
			Vector4 color = { 1.0f, 0.15f, 0.15f, 1.0f };

			missileLockOnReticleSprite_->SetColor(color);
			missileLockOnReticleSprite_->SetPosition({ finalX, finalY });
			missileLockOnReticleSprite_->SetSize({ baseReticleSize * aspectScaleX, baseReticleSize });
			missileLockOnReticleSprite_->Update();
			missileLockOnReticleSprite_->Draw();
		}
	}

	void DrawAimCursorOverlaySprite(Sprite* aimCursorSprite_, bool isJammed = false) {
		float minX = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (!GetOverlayBounds(minX, minY, maxX, maxY)) {
			return;
		}

		float winAppWidth = static_cast<float>(WinApp::GetClientWidth());
		float winAppHeight = static_cast<float>(WinApp::GetClientHeight());

		float width = maxX - minX;
		float height = maxY - minY;

		float spriteX = winAppWidth * 0.5f;
		float spriteY = winAppHeight * 0.5f;
		if (isJammed) {
			spriteX += (rand() % 11 - 5) * 1.5f;
			spriteY += (rand() % 11 - 5) * 1.5f;
		}
		const Vector2 center = { spriteX, spriteY };

		const float aspectScaleX = (height / width) * (winAppWidth / winAppHeight);

		if (aimCursorSprite_) {
			aimCursorSprite_->SetPosition(center);
			aimCursorSprite_->SetSize({ 44.0f * aspectScaleX, 44.0f });
			aimCursorSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 220.0f / 255.0f });
			aimCursorSprite_->Update();
			aimCursorSprite_->Draw();
		}
	}
}




