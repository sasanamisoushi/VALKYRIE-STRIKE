#include "MissilePresetManager.h"
#include "GamePlayScene.h"
#include "GamePlaySceneHelpers.h"
#include <externals/imgui/imgui.h>
#include <fstream>
#include "engine/Input/Input.h"
#include "engine/math/MyMath.h"

MissilePresetManager::MissilePresetManager(GamePlayScene* scene) : scene_(scene) {}

void MissilePresetManager::RefreshMissilePresetNames() {
	scene_->uiManager_->missilePresetNames_[0].clear();
	scene_->uiManager_->missilePresetNames_[1].clear();

	std::ifstream ifs(kMissilePresetsFilePath);
	if (!ifs.is_open()) {
		scene_->uiManager_->selectedMissilePresetIndex_[0] = 0;
		scene_->uiManager_->selectedMissilePresetIndex_[1] = 0;
		return;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		scene_->uiManager_->selectedMissilePresetIndex_[0] = 0;
		scene_->uiManager_->selectedMissilePresetIndex_[1] = 0;
		return;
	}

	const char *keys[] = { "normal", "homing" };
	for (int typeIndex = 0; typeIndex < 2; ++typeIndex) {
		if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
			continue;
		}

		for (const auto &presetData : root[keys[typeIndex]]) {
			if (presetData.is_object() && presetData.contains("name") && presetData["name"].is_string()) {
				scene_->uiManager_->missilePresetNames_[typeIndex].push_back(presetData["name"].get<std::string>());
			}
		}

		if (scene_->uiManager_->selectedMissilePresetIndex_[typeIndex] >= static_cast<int>(scene_->uiManager_->missilePresetNames_[typeIndex].size())) {
			scene_->uiManager_->selectedMissilePresetIndex_[typeIndex] = scene_->uiManager_->missilePresetNames_[typeIndex].empty()
				? 0
				: static_cast<int>(scene_->uiManager_->missilePresetNames_[typeIndex].size()) - 1;
		}
	}
}

bool MissilePresetManager::SaveMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName) {
	const int typeIndex = std::clamp(missileTypeIndex, 0, 1);
	const std::string trimmedName = TrimActionName(presetName);
	if (trimmedName.empty()) {
		scene_->uiManager_->missilePresetMessage_ = "保存名を入力してください。";
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

	const char *keys[] = { "normal", "homing" };
	if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
		root[keys[typeIndex]] = json::array();
	}

	json preset;
	preset["name"] = trimmedName;
	if (typeIndex == 0) {
		preset["speed"] = scene_->missileNormalSpeed;
		preset["scale"] = scene_->missileNormalScale;
		preset["collisionRadius"] = scene_->missileNormalCollisionRadius;
		preset["lifeTime"] = scene_->missileNormalLifeTime;
		if (scene_->ammoManager_) {
			preset["magazineCapacity"] = scene_->ammoManager_->GetNormalMagazineCapacity();
		}
	} else {
		preset["speed"] = scene_->missileSpeed;
		preset["homingStrength"] = scene_->missileHomingStrength;
		preset["scale"] = scene_->missileHomingScale;
		preset["collisionRadius"] = scene_->missileHomingCollisionRadius;
		preset["trailWidth"] = scene_->missileTrailWidth;
		preset["lifeTime"] = scene_->missileLifeTime;
		if (scene_->ammoManager_) {
			preset["magazineCapacity"] = scene_->ammoManager_->GetHomingMagazineCapacity();
		}
	}
	preset["muzzleOffset"] = scene_->missileMuzzleOffset;

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
		scene_->uiManager_->missilePresetMessage_ = "ミサイル設定を保存できませんでした。";
		return false;
	}

	ofs << root.dump(4);
	ofs.close();

	RefreshMissilePresetNames();
	for (size_t index = 0; index < scene_->uiManager_->missilePresetNames_[typeIndex].size(); ++index) {
		if (scene_->uiManager_->missilePresetNames_[typeIndex][index] == trimmedName) {
			scene_->uiManager_->selectedMissilePresetIndex_[typeIndex] = static_cast<int>(index);
			break;
		}
	}

	scene_->uiManager_->missilePresetMessage_ =
		std::string(typeIndex == 0 ? "通常弾" : "ホーミング") + "設定「" + trimmedName + "」を保存しました。";
	return true;
}

bool MissilePresetManager::ApplyMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName) {
	const int typeIndex = std::clamp(missileTypeIndex, 0, 1);
	const std::string trimmedName = TrimActionName(presetName);
	if (trimmedName.empty()) {
		scene_->uiManager_->missilePresetMessage_ = "読み込むミサイル設定名がありません。";
		return false;
	}

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		scene_->uiManager_->missilePresetMessage_ = "ミサイル設定ファイルが見つかりません。";
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		scene_->uiManager_->missilePresetMessage_ = "ミサイル設定ファイルを読み込めませんでした。";
		return false;
	}

	const char *keys[] = { "normal", "homing" };
	if (!root.contains(keys[typeIndex]) || !root[keys[typeIndex]].is_array()) {
		scene_->uiManager_->missilePresetMessage_ = "選択した種類の保存設定がありません。";
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
		scene_->uiManager_->missilePresetMessage_ = "選択したミサイル設定が見つかりません。";
		return false;
	}

	if (typeIndex == 0) {
		scene_->missileNormalSpeed = ReadJsonFloat(*preset, "speed", scene_->missileNormalSpeed);
		scene_->missileNormalScale = ReadJsonFloat(*preset, "scale", scene_->missileNormalScale);
		scene_->missileNormalCollisionRadius = ReadJsonFloat(*preset, "collisionRadius", scene_->missileNormalCollisionRadius);
		scene_->missileNormalLifeTime = ReadJsonInt(*preset, "lifeTime", scene_->missileNormalLifeTime);
		if (scene_->ammoManager_ && preset->contains("magazineCapacity")) {
			const int normCap = ReadJsonInt(*preset, "magazineCapacity", scene_->ammoManager_->GetNormalMagazineCapacity());
			scene_->ammoManager_->SetMagazineCapacities(normCap, scene_->ammoManager_->GetHomingMagazineCapacity());
		}
	} else {
		scene_->missileSpeed = ReadJsonFloat(*preset, "speed", scene_->missileSpeed);
		scene_->missileHomingStrength = ReadJsonFloat(*preset, "homingStrength", scene_->missileHomingStrength);
		scene_->missileHomingScale = ReadJsonFloat(*preset, "scale", scene_->missileHomingScale);
		scene_->missileHomingCollisionRadius = ReadJsonFloat(*preset, "collisionRadius", scene_->missileHomingCollisionRadius);
		scene_->missileTrailWidth = ReadJsonFloat(*preset, "trailWidth", scene_->missileTrailWidth);
		scene_->missileLifeTime = ReadJsonInt(*preset, "lifeTime", scene_->missileLifeTime);
		if (scene_->ammoManager_ && preset->contains("magazineCapacity")) {
			const int homCap = ReadJsonInt(*preset, "magazineCapacity", scene_->ammoManager_->GetHomingMagazineCapacity());
			scene_->ammoManager_->SetMagazineCapacities(scene_->ammoManager_->GetNormalMagazineCapacity(), homCap);
		}
	}
	scene_->missileMuzzleOffset = ReadJsonFloat(*preset, "muzzleOffset", scene_->missileMuzzleOffset);

	scene_->uiManager_->missilePresetMessage_ =
		std::string(typeIndex == 0 ? "通常弾" : "ホーミング") + "設定「" + trimmedName + "」を読み込みました。";
	return true;
}

void MissilePresetManager::DrawMissileSettingsUI() {
#ifdef ENABLE_IMGUI
	ImGui::Text("ミサイル設定");
	ImGui::DragFloat("発射位置距離", &scene_->missileMuzzleOffset, 0.05f, 0.0f, 5.0f, "%.2f");

	if (scene_ && scene_->uiManager_) {
		scene_->uiManager_->DrawAmmoSettingsUI();
	}

	ImGui::Separator();
	const char *presetTypes[] = { "通常弾", "ホーミング" };
	ImGui::Combo("保存する種類", &scene_->uiManager_->missilePresetTypeIndex_, presetTypes, IM_ARRAYSIZE(presetTypes));
	ImGui::InputText("ミサイル保存名", scene_->uiManager_->missilePresetName_, IM_ARRAYSIZE(scene_->uiManager_->missilePresetName_));
	if (ImGui::Button("この種類の設定を保存")) {
		SaveMissilePreset(kMissilePresetsFilePath, scene_->uiManager_->missilePresetTypeIndex_, scene_->uiManager_->missilePresetName_);
	}
	ImGui::SameLine();
	if (ImGui::Button("ミサイル保存一覧を更新")) {
		RefreshMissilePresetNames();
	}

	const int loadTypeIndex = std::clamp(scene_->uiManager_->missilePresetTypeIndex_, 0, 1);
	if (!scene_->uiManager_->missilePresetNames_[loadTypeIndex].empty()) {
		std::vector<const char *> presetItems;
		presetItems.reserve(scene_->uiManager_->missilePresetNames_[loadTypeIndex].size());
		for (const std::string &name : scene_->uiManager_->missilePresetNames_[loadTypeIndex]) {
			presetItems.push_back(name.c_str());
		}
		if (scene_->uiManager_->selectedMissilePresetIndex_[loadTypeIndex] >= static_cast<int>(presetItems.size())) {
			scene_->uiManager_->selectedMissilePresetIndex_[loadTypeIndex] = 0;
		}
		ImGui::Combo("読み込む設定", &scene_->uiManager_->selectedMissilePresetIndex_[loadTypeIndex], presetItems.data(), static_cast<int>(presetItems.size()));
		if (ImGui::Button("選択したミサイル設定を読み込む")) {
			ApplyMissilePreset(
				kMissilePresetsFilePath,
				loadTypeIndex,
				scene_->uiManager_->missilePresetNames_[loadTypeIndex][scene_->uiManager_->selectedMissilePresetIndex_[loadTypeIndex]]);
		}
	} else {
		ImGui::TextDisabled("%sの保存設定はまだありません。", presetTypes[loadTypeIndex]);
	}
	if (!scene_->uiManager_->missilePresetMessage_.empty()) {
		ImGui::TextWrapped("%s", scene_->uiManager_->missilePresetMessage_.c_str());
	}

	ImGui::Separator();
	ImGui::Text("通常弾（左クリック）");
	ImGui::DragFloat("通常弾速度", &scene_->missileNormalSpeed, 0.05f, 0.1f, 6.0f, "%.2f");
	ImGui::DragFloat("通常弾サイズ", &scene_->missileNormalScale, 0.01f, 0.05f, 2.0f, "%.2f");
	ImGui::DragFloat("通常弾当たり判定", &scene_->missileNormalCollisionRadius, 0.01f, 0.05f, 2.0f, "%.2f");
	ImGui::SliderInt("通常弾寿命", &scene_->missileNormalLifeTime, 10, 900);

	ImGui::Separator();
	ImGui::Text("ホーミングミサイル（右クリック）");
	ImGui::DragFloat("追尾速度", &scene_->missileSpeed, 0.05f, 0.1f, 6.0f, "%.2f");
	ImGui::DragFloat("曲がりやすさ", &scene_->missileHomingStrength, 0.005f, 0.0f, 0.6f, "%.3f");
	ImGui::DragFloat("ミサイルサイズ", &scene_->missileHomingScale, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::DragFloat("ミサイル当たり判定", &scene_->missileHomingCollisionRadius, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::DragFloat("煙の太さ", &scene_->missileTrailWidth, 0.01f, 0.05f, 3.0f, "%.2f");
	ImGui::SliderInt("ミサイル寿命", &scene_->missileLifeTime, 10, 1200);

	ImGui::Separator();
	if (ImGui::Button("通常弾をテスト発射")) {
		if (scene_->IsSimulationMode()) {
			scene_->isEditorPreviewPlaying_ = true;
		}
		FirePlayerMissile(MissileType::Normal);
	}
	ImGui::SameLine();
	if (ImGui::Button("ホーミングをテスト発射")) {
		if (scene_->IsSimulationMode()) {
			scene_->isEditorPreviewPlaying_ = true;
		}
		FirePlayerMissile(MissileType::MissileWithTrail);
	}
	ImGui::TextDisabled("選択中だけ確認では、弾はこのテストボタンを押した時だけ出ます。");
#endif
}

bool MissilePresetManager::FirePlayerMissile(MissileType type, Enemy *target, float horizontalOffset, bool consumeAmmo) {
	if (!scene_->player_ || !scene_->missileManager_) {
		return false;
	}
	if (consumeAmmo && !scene_->TryConsumeAmmo(type)) {
		return false;
	}

	const MissileTuning tuning = scene_->MakeMissileTuning(type);
	const Vector3 playerPos = scene_->player_->GetPosition();
	// 必殺技中は、固定された機体の向きではなく操作中のカメラ照準方向へ発射する。
	const Vector3 forward = NormalizeOrVector3(scene_->player_->GetAttackDirection(), { 0.0f, 0.0f, 1.0f });
	Vector3 right = NormalizeOrVector3(MyMath::Cross({ 0.0f, 1.0f, 0.0f }, forward), { 1.0f, 0.0f, 0.0f });
	const float muzzleOffset = (std::max)(0.0f, scene_->missileMuzzleOffset);
	const Vector3 muzzlePos = {
		playerPos.x + forward.x * muzzleOffset + right.x * horizontalOffset,
		playerPos.y + forward.y * muzzleOffset,
		playerPos.z + forward.z * muzzleOffset + right.z * horizontalOffset,
	};

	Vector3 fireDirection = forward;
	// ロックオン対象がある弾は、形態を問わず最初から対象へ向けて発射する。
	// 通常弾も飛行中に弱く追尾するため、ファイター形態で照準がわずかに遅れても外れない。
	const bool shouldAim = target != nullptr;

	if (shouldAim) {
		Vector3 targetPosition = target->GetPosition();
		float collisionRadius = 1.0f;
		try {
			collisionRadius = target->GetCollisionRadius();
		} catch (...) {}
		targetPosition.y += collisionRadius * 0.3f;
		fireDirection = NormalizeOrVector3(SubtractVector3(targetPosition, muzzlePos), forward);
	}

	const Vector3 playerVelocity = scene_->player_->GetVelocity();
	const Vector3 velocity = {
		fireDirection.x * (std::max)(0.01f, tuning.speed) + playerVelocity.x,
		fireDirection.y * (std::max)(0.01f, tuning.speed) + playerVelocity.y,
		fireDirection.z * (std::max)(0.01f, tuning.speed) + playerVelocity.z,
	};

	scene_->missileManager_->Shoot(muzzlePos, velocity, type, tuning, target);
	return true;
}

