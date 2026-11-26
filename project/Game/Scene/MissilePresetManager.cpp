#include "MissilePresetManager.h"
#include "AmmoManager.h"
#include "Game/bullet/MissileManager.h"
#include "GamePlaySceneHelpers.h"
#include <externals/imgui/imgui.h>
#include <fstream>
#include "engine/Input/Input.h"
#include "engine/base/Logger.h"
#include "engine/math/MyMath.h"

MissilePresetManager::MissilePresetManager(MissilePresetSceneContext context)
	: context_(std::move(context)) {}

void MissilePresetManager::RefreshMissilePresetNames() {
	missilePresetNames_[0].clear();
	missilePresetNames_[1].clear();

	std::ifstream ifs(kMissilePresetsFilePath);
	if (!ifs.is_open()) {
		selectedMissilePresetIndex_[0] = 0;
		selectedMissilePresetIndex_[1] = 0;
		return;
	}

	json root;
	try {
		ifs >> root;
	} catch (const std::exception& e) {
		GetLogger().Log(std::string("[MissilePresetManager] Failed to parse presets: ") + e.what());
		selectedMissilePresetIndex_[0] = 0;
		selectedMissilePresetIndex_[1] = 0;
		return;
	}

	const char *keys[] = { "normal", "homing" };
	for (int typeIndex = 0; typeIndex < 2; ++typeIndex) {
		if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
			continue;
		}

		for (const auto &presetData : root[keys[typeIndex]]) {
			if (presetData.is_object() && presetData.contains("name") && presetData["name"].is_string()) {
				missilePresetNames_[typeIndex].push_back(presetData["name"].get<std::string>());
			}
		}

		if (selectedMissilePresetIndex_[typeIndex] >= static_cast<int>(missilePresetNames_[typeIndex].size())) {
			selectedMissilePresetIndex_[typeIndex] = missilePresetNames_[typeIndex].empty()
				? 0
				: static_cast<int>(missilePresetNames_[typeIndex].size()) - 1;
		}
	}
}

bool MissilePresetManager::SaveMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName) {
	const int typeIndex = std::clamp(missileTypeIndex, 0, 1);
	const std::string trimmedName = TrimActionName(presetName);
	if (trimmedName.empty()) {
		missilePresetMessage_ = "保存名を入力してください。";
		return false;
	}

	json root;
	{
		std::ifstream ifs(filePath);
		if (ifs.is_open()) {
			try {
				ifs >> root;
			} catch (const std::exception& e) {
				GetLogger().Log(std::string("[MissilePresetManager] Resetting unreadable preset file: ") + e.what());
				root = json::object();
			}
		}
	}
	if (!root.is_object()) {
		root = json::object();
	}

	const char *keys[] = { "normal", "homing" };
	if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
		root[keys[typeIndex]] = json::array();
	}

	json preset;
	preset["name"] = trimmedName;
	if (typeIndex == 0) {
		preset["speed"] = *context_.missileNormalSpeed;
		preset["scale"] = *context_.missileNormalScale;
		preset["collisionRadius"] = *context_.missileNormalCollisionRadius;
		preset["lifeTime"] = *context_.missileNormalLifeTime;
		if (context_.ammoManager) {
			preset["magazineCapacity"] = context_.ammoManager->GetNormalMagazineCapacity();
		}
	} else {
		preset["speed"] = *context_.missileSpeed;
		preset["homingStrength"] = *context_.missileHomingStrength;
		preset["scale"] = *context_.missileHomingScale;
		preset["collisionRadius"] = *context_.missileHomingCollisionRadius;
		preset["trailWidth"] = *context_.missileTrailWidth;
		preset["lifeTime"] = *context_.missileLifeTime;
		if (context_.ammoManager) {
			preset["magazineCapacity"] = context_.ammoManager->GetHomingMagazineCapacity();
		}
	}
	preset["muzzleOffset"] = *context_.missileMuzzleOffset;

	json &presets = root[keys[typeIndex]];
	bool updated = false;
	for (auto &existingPreset : presets) {
		if (existingPreset.is_object() && existingPreset.value("name", "") == trimmedName) {
			existingPreset = preset;
			updated = true;
			break;
		}
	}
	if (!updated) {
		presets.push_back(preset);
	}

	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		missilePresetMessage_ = "ミサイル設定を保存できませんでした。";
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	RefreshMissilePresetNames();
	for (size_t index = 0; index < missilePresetNames_[typeIndex].size(); ++index) {
		if (missilePresetNames_[typeIndex][index] == trimmedName) {
			selectedMissilePresetIndex_[typeIndex] = static_cast<int>(index);
			break;
		}
	}

	missilePresetMessage_ =
		std::string(typeIndex == 0 ? "通常弾" : "ホーミング") + "設定「" + trimmedName + "」を保存しました。";
	return true;
}

bool MissilePresetManager::ApplyMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName) {
	const int typeIndex = std::clamp(missileTypeIndex, 0, 1);
	const std::string trimmedName = TrimActionName(presetName);
	if (trimmedName.empty()) {
		missilePresetMessage_ = "読み込むミサイル設定名がありません。";
		return false;
	}

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		missilePresetMessage_ = "ミサイル設定ファイルが見つかりません。";
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (const std::exception& e) {
		GetLogger().Log(std::string("[MissilePresetManager] Failed to apply preset: ") + e.what());
		missilePresetMessage_ = "ミサイル設定ファイルを読み込めませんでした。";
		return false;
	}

	const char *keys[] = { "normal", "homing" };
	if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
		missilePresetMessage_ = "選択した種類の保存設定がありません。";
		return false;
	}

	const json *preset = nullptr;
	for (const auto &presetData : root[keys[typeIndex]]) {
		if (presetData.is_object() && presetData.value("name", "") == trimmedName) {
			preset = &presetData;
			break;
		}
	}
	if (!preset) {
		missilePresetMessage_ = "選択したミサイル設定が見つかりません。";
		return false;
	}

	if (typeIndex == 0) {
		*context_.missileNormalSpeed = ReadJsonFloat(*preset, "speed", *context_.missileNormalSpeed);
		*context_.missileNormalScale = ReadJsonFloat(*preset, "scale", *context_.missileNormalScale);
		*context_.missileNormalCollisionRadius = ReadJsonFloat(*preset, "collisionRadius", *context_.missileNormalCollisionRadius);
		*context_.missileNormalLifeTime = ReadJsonInt(*preset, "lifeTime", *context_.missileNormalLifeTime);
		if (context_.ammoManager && preset->contains("magazineCapacity")) {
			const int normCap = ReadJsonInt(*preset, "magazineCapacity", context_.ammoManager->GetNormalMagazineCapacity());
			context_.ammoManager->SetMagazineCapacities(normCap, context_.ammoManager->GetHomingMagazineCapacity());
		}
	} else {
		*context_.missileSpeed = ReadJsonFloat(*preset, "speed", *context_.missileSpeed);
		*context_.missileHomingStrength = ReadJsonFloat(*preset, "homingStrength", *context_.missileHomingStrength);
		*context_.missileHomingScale = ReadJsonFloat(*preset, "scale", *context_.missileHomingScale);
		*context_.missileHomingCollisionRadius = ReadJsonFloat(*preset, "collisionRadius", *context_.missileHomingCollisionRadius);
		*context_.missileTrailWidth = ReadJsonFloat(*preset, "trailWidth", *context_.missileTrailWidth);
		*context_.missileLifeTime = ReadJsonInt(*preset, "lifeTime", *context_.missileLifeTime);
		if (context_.ammoManager && preset->contains("magazineCapacity")) {
			const int homCap = ReadJsonInt(*preset, "magazineCapacity", context_.ammoManager->GetHomingMagazineCapacity());
			context_.ammoManager->SetMagazineCapacities(context_.ammoManager->GetNormalMagazineCapacity(), homCap);
		}
	}
	*context_.missileMuzzleOffset = ReadJsonFloat(*preset, "muzzleOffset", *context_.missileMuzzleOffset);

	missilePresetMessage_ =
		std::string(typeIndex == 0 ? "通常弾" : "ホーミング") + "設定「" + trimmedName + "」を読み込みました。";
	return true;
}

void MissilePresetManager::DrawMissileSettingsUI() {
#ifdef ENABLE_IMGUI
	ImGui::Text("ミサイル設定");
	ImGui::DragFloat("発射位置距離", context_.missileMuzzleOffset, 0.05f, 0.0f, 5.0f, "%.2f");

	if (context_.drawAmmoSettingsUI) {
		context_.drawAmmoSettingsUI();
	}

	ImGui::Separator();
	const char *presetTypes[] = { "通常弾", "ホーミング" };
	ImGui::Combo("保存する種類", &missilePresetTypeIndex_, presetTypes, IM_ARRAYSIZE(presetTypes));
	ImGui::InputText("ミサイル保存名", missilePresetName_, IM_ARRAYSIZE(missilePresetName_));
	if (ImGui::Button("この種類の設定を保存")) {
		SaveMissilePreset(kMissilePresetsFilePath, missilePresetTypeIndex_, missilePresetName_);
	}
	ImGui::SameLine();
	if (ImGui::Button("ミサイル保存一覧を更新")) {
		RefreshMissilePresetNames();
	}

	const int loadTypeIndex = std::clamp(missilePresetTypeIndex_, 0, 1);
	if (!missilePresetNames_[loadTypeIndex].empty()) {
		std::vector<const char *> presetItems;
		presetItems.reserve(missilePresetNames_[loadTypeIndex].size());
		for (const std::string &name : missilePresetNames_[loadTypeIndex]) {
			presetItems.push_back(name.c_str());
		}
		if (selectedMissilePresetIndex_[loadTypeIndex] >= static_cast<int>(presetItems.size())) {
			selectedMissilePresetIndex_[loadTypeIndex] = 0;
		}
		ImGui::Combo("読み込む設定", &selectedMissilePresetIndex_[loadTypeIndex], presetItems.data(), static_cast<int>(presetItems.size()));
		if (ImGui::Button("選択したミサイル設定を読み込む")) {
			ApplyMissilePreset(
				kMissilePresetsFilePath,
				loadTypeIndex,
				missilePresetNames_[loadTypeIndex][selectedMissilePresetIndex_[loadTypeIndex]]);
		}
	} else {
		ImGui::TextDisabled("%sの保存設定はまだありません。", presetTypes[loadTypeIndex]);
	}
	if (!missilePresetMessage_.empty()) {
		ImGui::TextWrapped("%s", missilePresetMessage_.c_str());
	}

	ImGui::Separator();
	ImGui::Text("通常弾（左クリック）");
	ImGui::DragFloat("通常弾速度", context_.missileNormalSpeed, 0.05f, 0.1f, 6.0f, "%.2f");
	ImGui::DragFloat("通常弾サイズ", context_.missileNormalScale, 0.01f, 0.05f, 2.0f, "%.2f");
	ImGui::DragFloat("通常弾当たり判定", context_.missileNormalCollisionRadius, 0.01f, 0.05f, 2.0f, "%.2f");
	ImGui::SliderInt("通常弾寿命", context_.missileNormalLifeTime, 10, 900);

	ImGui::Separator();
	ImGui::Text("ホーミングミサイル（右クリック）");
	ImGui::DragFloat("追尾速度", context_.missileSpeed, 0.05f, 0.1f, 6.0f, "%.2f");
	ImGui::DragFloat("曲がりやすさ", context_.missileHomingStrength, 0.005f, 0.0f, 0.6f, "%.3f");
	ImGui::DragFloat("ミサイルサイズ", context_.missileHomingScale, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::DragFloat("ミサイル当たり判定", context_.missileHomingCollisionRadius, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::DragFloat("煙の太さ", context_.missileTrailWidth, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::SliderInt("ミサイル寿命", context_.missileLifeTime, 10, 1200);

	ImGui::Separator();
	if (ImGui::Button("通常弾をテスト発射")) {
		FirePlayerMissile(MissileType::Normal);
	}
	ImGui::SameLine();
	if (ImGui::Button("ホーミングをテスト発射")) {
		FirePlayerMissile(MissileType::MissileWithTrail);
	}
	ImGui::TextDisabled("選択中だけ確認では、弾はこのテストボタンを押した時だけ出ます。");
#endif
}

bool MissilePresetManager::FirePlayerMissile(MissileType type, Enemy *target, float horizontalOffset, bool consumeAmmo,
	const Vector3* targetPoint) {
	if (!context_.player || !context_.missileManager || !context_.makeMissileTuning) {
		return false;
	}
	if (consumeAmmo && (!context_.tryConsumeAmmo || !context_.tryConsumeAmmo(type))) {
		return false;
	}

	const MissileTuning tuning = context_.makeMissileTuning(type);
	const Vector3 playerPos = context_.player->GetPosition();
	// 必殺技中は、固定された機体の向きではなく操作中のカメラ照準方向へ発射する。
	const Vector3 forward = NormalizeOrVector3(context_.player->GetAttackDirection(), { 0.0f, 0.0f, 1.0f });
	Vector3 right = NormalizeOrVector3(MyMath::Cross({ 0.0f, 1.0f, 0.0f }, forward), { 1.0f, 0.0f, 0.0f });
	const float muzzleOffset = (std::max)(0.0f, context_.missileMuzzleOffset ? *context_.missileMuzzleOffset : 0.0f);
	const Vector3 muzzlePos = {
		playerPos.x + forward.x * muzzleOffset + right.x * horizontalOffset,
		playerPos.y + forward.y * muzzleOffset,
		playerPos.z + forward.z * muzzleOffset + right.z * horizontalOffset,
	};

	Vector3 fireDirection = forward;
	// ロックオン対象がある弾は、形態を問わず最初から対象へ向けて発射する。
	// 通常弾も飛行中に弱く追尾するため、ファイター形態で照準がわずかに遅れても外れない。
	const bool shouldAim = target != nullptr || targetPoint != nullptr;

	if (shouldAim) {
		Vector3 targetPosition = targetPoint ? *targetPoint : target->GetAimPosition();
		if (!targetPoint) {
			const float collisionRadius = target->GetCollisionRadius();
			targetPosition.y += collisionRadius * 0.3f;
		}
		fireDirection = NormalizeOrVector3(SubtractVector3(targetPosition, muzzlePos), forward);
	}

	const Vector3 playerVelocity = context_.player->GetVelocity();
	const Vector3 velocity = {
		fireDirection.x * (std::max)(0.01f, tuning.speed) + playerVelocity.x,
		fireDirection.y * (std::max)(0.01f, tuning.speed) + playerVelocity.y,
		fireDirection.z * (std::max)(0.01f, tuning.speed) + playerVelocity.z,
	};

	context_.missileManager->Shoot(muzzlePos, velocity, type, tuning, target, targetPoint);
	return true;
}

