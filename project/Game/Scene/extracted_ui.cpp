void GamePlayScene::RefreshSimulationActionNames() {
	simulationActionNames_.clear();

	std::ifstream ifs(kSimulationActionsFilePath);
	if (!ifs.is_open()) {
		selectedSimulationActionIndex_ = 0;
		return;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		selectedSimulationActionIndex_ = 0;
		return;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		selectedSimulationActionIndex_ = 0;
		return;
	}

	for (const auto &actionData : root["actions"]) {
		if (!actionData.is_object() || !actionData.contains("name") || !actionData["name"].is_string()) {
			continue;
		}

		simulationActionNames_.push_back(actionData["name"].get<std::string>());
	}

	if (selectedSimulationActionIndex_ >= static_cast<int>(simulationActionNames_.size())) {
		selectedSimulationActionIndex_ = simulationActionNames_.empty() ? 0 : static_cast<int>(simulationActionNames_.size()) - 1;
	}
}

bool GamePlayScene::ApplySimulationAction(const std::string &filePath, const std::string &actionName) {
	const std::string trimmedName = TrimActionName(actionName);
	if (trimmedName.empty()) {
		simulationActionMessage_ = "読み込む行動名がありません。";
		return false;
	}

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		simulationActionMessage_ = "名前付き行動ファイルが見つかりません。";
		return false;
	}

	json root;
	try {
		ifs >> root;
	} catch (...) {
		simulationActionMessage_ = "名前付き行動ファイルを読み込めませんでした。";
		return false;
	}

	if (!root.contains("actions") || !root["actions"].is_array()) {
		simulationActionMessage_ = "保存された行動がありません。";
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
		simulationActionMessage_ = "選択した行動が見つかりません。";
		return false;
	}

	lockedEnemy_ = nullptr;
	aimAssistEnemy_ = nullptr;
	CancelMultiLock();
	isCinematicLockOnCameraInitialized_ = false;
	isGameOver_ = false;
	gameOverTimer_ = 0;

	if (player_ && action->contains("player") && (*action)["player"].is_object()) {
		const json &playerData = (*action)["player"];
		if (playerData.contains("modeParams") && playerData["modeParams"].is_array()) {
			const json &modeParams = playerData["modeParams"];
			for (int modeIndex = 0; modeIndex < 3 && modeIndex < static_cast<int>(modeParams.size()); ++modeIndex) {
				PlayerModeParams params = player_->GetModeParams(static_cast<PlayerMode>(modeIndex));
				ApplyPlayerModeParamsFromJson(modeParams[modeIndex], params);
				player_->SetModeParams(static_cast<PlayerMode>(modeIndex), params);
			}
		}

		const int modeIndex = std::clamp(ReadJsonInt(playerData, "mode", 0), 0, 2);
		player_->ChangeMode(static_cast<PlayerMode>(modeIndex));
		player_->SetPosition(ReadVector3Json(playerData.value("position", json::array()), player_->GetPosition()));
		player_->SetRotation(ReadVector3Json(playerData.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f }));
	}

	if (action->contains("missile") && (*action)["missile"].is_object()) {
		const json &missileData = (*action)["missile"];
		missileNormalSpeed = ReadJsonFloat(missileData, "normalSpeed", missileNormalSpeed);
		missileNormalScale = ReadJsonFloat(missileData, "normalScale", missileNormalScale);
		missileNormalCollisionRadius = ReadJsonFloat(missileData, "normalCollisionRadius", missileNormalCollisionRadius);
		missileNormalLifeTime = ReadJsonInt(missileData, "normalLifeTime", missileNormalLifeTime);
		missileSpeed = ReadJsonFloat(missileData, "speed", missileSpeed);
		missileAmpX = ReadJsonFloat(missileData, "ampX", missileAmpX);
		missileAmpZ = ReadJsonFloat(missileData, "ampZ", missileAmpZ);
		missileAmpY = ReadJsonFloat(missileData, "ampY", missileAmpY);
		missileFreqY = ReadJsonFloat(missileData, "freqY", missileFreqY);
		missileBaseY = ReadJsonFloat(missileData, "baseY", missileBaseY);
		missileHomingStrength = ReadJsonFloat(missileData, "homingStrength", missileHomingStrength);
		missileHomingScale = ReadJsonFloat(missileData, "homingScale", missileHomingScale);
		missileHomingCollisionRadius = ReadJsonFloat(missileData, "homingCollisionRadius", missileHomingCollisionRadius);
		missileTrailWidth = ReadJsonFloat(missileData, "trailWidth", missileTrailWidth);
		missileLifeTime = ReadJsonInt(missileData, "lifeTime", missileLifeTime);
		missileMuzzleOffset = ReadJsonFloat(missileData, "muzzleOffset", missileMuzzleOffset);
	}

	if (explosionManager_ && action->contains("explosion") && (*action)["explosion"].is_object()) {
		const json &explosionData = (*action)["explosion"];
		auto &config = explosionManager_->GetConfig();
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
		auto obstacleIt = obstacles_.begin();
		for (const auto &obstacleData : (*action)["obstacles"]) {
			if (obstacleIt == obstacles_.end()) {
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
		aimAssistEnemy_ = nullptr;
		CancelMultiLock();
		enemies_.clear();
		enemySpawns_.clear();

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
			enemySpawns_.push_back(spawnData);

			if (spawnData.isInitialSpawn) {
				auto enemy = std::make_unique<Enemy>();
				enemy->Initialize(spawnData.position);
				enemy->SetRotation(spawnData.rotation);
				enemy->SetScale(ReadVector3Json(enemyData.value("scale", json::array()), enemy->GetScale()));
				enemy->SetSpawnPointIndex(enemySpawns_.size() - 1);
				enemies_.push_back(std::move(enemy));
			}
		}
		enemyRespawnTimers_.assign(enemySpawns_.size(), kNoRespawnTimer);
	}

	if (missileManager_) {
		missileManager_->Initialize();
	}
	if (enemyBulletManager_) {
		enemyBulletManager_->Initialize();
	}

	if (!isDebugCameraActive_ && camera && player_) {
		UpdateGameplayCamera();
		camera->Update();
	} else if (debugFlyCamera_) {
		debugFlyCamera_->Camera::Update();
	}

	Camera *activeCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
	if (player_) {
		player_->UpdateModel();
	}
	for (auto &enemy : enemies_) {
		enemy->UpdateModel();
	}
	for (auto &obstacle : obstacles_) {
		obstacle->Update();
	}
	if (missileManager_) {
		missileManager_->UpdateModels(activeCamera);
	}

	simulationActionMessage_ = "設定「" + trimmedName + "」をゲームに読み込みました。";
	return true;
}

void GamePlayScene::RefreshMissilePresetNames() {
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
	} catch (...) {
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

bool GamePlayScene::SaveMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName) {
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
		preset["speed"] = missileNormalSpeed;
		preset["scale"] = missileNormalScale;
		preset["collisionRadius"] = missileNormalCollisionRadius;
		preset["lifeTime"] = missileNormalLifeTime;
	} else {
		preset["speed"] = missileSpeed;
		preset["homingStrength"] = missileHomingStrength;
		preset["scale"] = missileHomingScale;
		preset["collisionRadius"] = missileHomingCollisionRadius;
		preset["trailWidth"] = missileTrailWidth;
		preset["lifeTime"] = missileLifeTime;
	}
	preset["muzzleOffset"] = missileMuzzleOffset;

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

bool GamePlayScene::ApplyMissilePreset(const std::string &filePath, int missileTypeIndex, const std::string &presetName) {
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
	} catch (...) {
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
		missileNormalSpeed = ReadJsonFloat(*preset, "speed", missileNormalSpeed);
		missileNormalScale = ReadJsonFloat(*preset, "scale", missileNormalScale);
		missileNormalCollisionRadius = ReadJsonFloat(*preset, "collisionRadius", missileNormalCollisionRadius);
		missileNormalLifeTime = ReadJsonInt(*preset, "lifeTime", missileNormalLifeTime);
	} else {
		missileSpeed = ReadJsonFloat(*preset, "speed", missileSpeed);
		missileHomingStrength = ReadJsonFloat(*preset, "homingStrength", missileHomingStrength);
		missileHomingScale = ReadJsonFloat(*preset, "scale", missileHomingScale);
		missileHomingCollisionRadius = ReadJsonFloat(*preset, "collisionRadius", missileHomingCollisionRadius);
		missileTrailWidth = ReadJsonFloat(*preset, "trailWidth", missileTrailWidth);
		missileLifeTime = ReadJsonInt(*preset, "lifeTime", missileLifeTime);
	}
	missileMuzzleOffset = ReadJsonFloat(*preset, "muzzleOffset", missileMuzzleOffset);

	missilePresetMessage_ =
		std::string(typeIndex == 0 ? "通常弾" : "ホーミング") + "設定「" + trimmedName + "」を読み込みました。";
	return true;
}

void GamePlayScene::DrawSimulationSaveControls() {
#ifdef ENABLE_IMGUI
	ImGui::Separator();
	ImGui::InputText("保存名", simulationActionName_, IM_ARRAYSIZE(simulationActionName_));
	if (ImGui::Button("名前を付けて行動を保存")) {
		SaveNamedSimulationAction(kSimulationActionsFilePath, simulationActionName_);
	}

void GamePlayScene::DrawGameplayActionControls() {
#ifdef ENABLE_IMGUI
	ImGui::Separator();
	ImGui::Text("保存済みシミュレーション設定");
	ImGui::TextWrapped("シミュレーション画面で保存した内容を、現在のゲーム側の設定値として読み込みます。");
	if (ImGui::Button("保存一覧を更新")) {
		RefreshSimulationActionNames();
	}

	if (simulationActionNames_.empty()) {
		ImGui::TextDisabled("保存されたシミュレーション設定がありません。");
		if (!simulationActionMessage_.empty()) {
			ImGui::TextWrapped("%s", simulationActionMessage_.c_str());
		}
		return;
	}

	std::vector<const char *> actionNameItems;
	actionNameItems.reserve(simulationActionNames_.size());
	for (const std::string &name : simulationActionNames_) {
		actionNameItems.push_back(name.c_str());
	}

	if (selectedSimulationActionIndex_ >= static_cast<int>(actionNameItems.size())) {
		selectedSimulationActionIndex_ = 0;
	}

	ImGui::Combo("読み込む設定", &selectedSimulationActionIndex_, actionNameItems.data(), static_cast<int>(actionNameItems.size()));
	if (ImGui::Button("この設定をゲームに読み込む")) {
		ApplySimulationAction(kSimulationActionsFilePath, simulationActionNames_[selectedSimulationActionIndex_]);
	}

	if (!simulationActionMessage_.empty()) {
		ImGui::TextWrapped("%s", simulationActionMessage_.c_str());
	}
#endif
}

void GamePlayScene::DrawMissileSettingsUI() {
#ifdef ENABLE_IMGUI
	ImGui::Text("ミサイル設定");
	ImGui::DragFloat("発射位置距離", &missileMuzzleOffset, 0.05f, 0.0f, 5.0f, "%.2f");

	ImGui::Separator();
	const char *presetTypes[] = { "通常弾", "ホーミング" };
	ImGui::Combo("保存する種類", &missilePresetTypeIndex_, presetTypes, IM_ARRAYSIZE(presetTypes));
	ImGui::InputText("ミサイル保存名", missilePresetName_, IM_ARRAYSIZE(missilePresetName_));
	if (ImGui::Button("この種類の設定を保存")) {
		SaveMissilePreset(kMissilePresetsFilePath, missilePresetTypeIndex_, missilePresetName_);
	}

void GamePlayScene::DrawSimulationScreenUI() {
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
	ImGui::SetNextWindowPos(ImVec2(panelX, 20.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

	const ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse;

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
		ResetEditorPreview();
	}
	ImGui::SameLine();
	if (ImGui::Button("再生")) {
		isEditorPreviewPlaying_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("停止")) {
		isEditorPreviewPlaying_ = false;
	}
	ImGui::SameLine();
	ImGui::TextColored(
		isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
		"状態: %s",
		isEditorPreviewPlaying_ ? "再生中" : "停止中");

	ImGui::Separator();
	const char *previewModes[] = { "選択中だけ確認", "全体確認" };
	ImGui::Combo("確認モード", &simulationPlaybackMode_, previewModes, IM_ARRAYSIZE(previewModes));
	if (simulationPlaybackMode_ == 0) {
		ImGui::TextDisabled("今選んでいるカテゴリだけ動きます。ミサイルはテスト発射ボタンでだけ出ます。");
	} else {
		ImGui::TextDisabled("プレイヤー・敵・ミサイルをまとめて動かして全体の流れを確認します。");
	}

	ImGui::Separator();
	ImGui::Text("カメラ: %s", isDebugCameraActive_ ? "フリーカメラ" : "プレイヤー視点");
	ImGui::SameLine();
	if (isDebugCameraActive_) {
		if (ImGui::Button("プレイヤー視点にする (F3)")) {
			SetDebugCameraActive(false);
		}
	} else {
		if (ImGui::Button("フリーカメラに戻す (F3)")) {
			SetDebugCameraActive(true);
		}
	}

	DrawSimulationSaveControls();

	ImGui::Separator();
	const char *categories[] = { "プレイヤー", "ミサイル", "敵 & イベント", "パーティクル", "カメラ" };
	ImGui::Combo("カテゴリ", &currentSimulationTarget_, categories, IM_ARRAYSIZE(categories));
	ImGui::Separator();

	if (currentSimulationTarget_ == 0) {
		ImGui::Text("プレイヤー移動設定");
		if (player_) {
			auto mode = player_->GetCurrentMode();
			const char* modeName = (mode == PlayerMode::Fighter) ? "ファイター (1キー)" : 
								   (mode == PlayerMode::Gerwalk) ? "ガウォーク (2キー)" : "バトロイド (3キー)";
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "現在の形態: %s", modeName);
			
			PlayerModeParams p = player_->GetModeParams(mode);
			ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
			ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
			ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
			ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);
			player_->SetModeParams(mode, p);

			ImGui::Separator();
			ImGui::Text("アニメーションデバッグ");
			bool isDebug = player_->IsAnimDebugActive();
			if (ImGui::Checkbox("デバッグ時間を強制", &isDebug)) {
				player_->SetAnimDebugActive(isDebug);
			}
			if (isDebug) {
				float t = player_->GetTargetAnimationTime();
				float duration = player_->GetAnimationDuration();
				if (ImGui::SliderFloat("アニメーション時間 (秒)", &t, 0.0f, duration, "%.3f秒")) {
					player_->SetTargetAnimationTime(t);
				}
				ImGui::Text("対応フレーム (24fps換算): %.1f", t * 24.0f);
			} else {
				ImGui::Text("アニメーション時間: %.3f秒 (目標: %.3f秒)", player_->GetAnimationTime(), player_->GetTargetAnimationTime());
			}
		} else {
			ImGui::Text("プレイヤーが初期化されていません。");
		}
	} else if (currentSimulationTarget_ == 1) {
		DrawMissileSettingsUI();
	} else if (currentSimulationTarget_ == 2) {
		ImGui::Text("=== 敵の出現とルート ===");
		ImGui::Text("Lock-on: %s", lockedEnemy_ ? "LOCKED" : "NONE");
		ImGui::Text("Tab: ターゲットロック / X: ロック解除 / F2: シミュレーションを閉じる");
		ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

		if (ImGui::Button("敵を生成する")) {
			auto newEnemy = std::make_unique<Enemy>();
			newEnemy->Initialize({ newEnemyPos[0], newEnemyPos[1], newEnemyPos[2] });
			enemies_.push_back(std::move(newEnemy));
		}

		ImGui::Separator();
		ImGui::Text("敵のリスト (総数: %d)", static_cast<int>(enemies_.size()));
		int index = 0;
		for (const auto &enemy : enemies_) {
			Vector3 pos = enemy->GetPosition();
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
			++index;
		}

		ImGui::Separator();
		ImGui::Text("イベントツール：増援設定");
		static int selectedTrigger = 0;
		static int selectedTarget = 0;
		static int eventDelay = 60;
		std::vector<const char *> enemyNames;
		for (const auto &spawn : enemySpawns_) {
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
				enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
				enemySpawns_[selectedTarget].isInitialSpawn = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("イベントを保存")) {
				enemyEventManager_.SaveEvents("resources/enemy_events.json");
			}

			for (size_t i = 0; i < enemyEventManager_.GetEvents().size(); ++i) {
				const auto &event = enemyEventManager_.GetEvents()[i];
				ImGui::Text("[%d] %s が死んだら %d F後に %s が出現",
					static_cast<int>(i),
					event.triggerEnemyName.c_str(),
					event.delayFrames,
					event.targetEnemyName.c_str());
				ImGui::SameLine();
				if (ImGui::Button(("削除##event" + std::to_string(i)).c_str())) {
					enemyEventManager_.RemoveEvent(i);
					break;
				}
			}
		} else {
			ImGui::Text("敵の出現データがありません。");
		}

void GamePlayScene::UpdateUI() {
#ifdef ENABLE_IMGUI
	if (ImGuiManager::IsVisible()) {
		if (IsSimulationMode()) {
			DrawSimulationScreenUI();
			return;
		}

		ImGui::Begin("Simulation");
		ImGui::Text("シミュレーション設定");
		ImGui::TextWrapped("保存済み設定の読み込みはここで行えます。細かい保存や確認は専用画面を開いてください。");
		if (ImGui::Button("シミュレーション画面を開く (F2)")) {
			LaunchSimulationExecutable();
		}
		DrawGameplayActionControls();
		ImGui::End();
		if (false) {

		// --- シミュレーション起動用ミニウィンドウ ---
		ImGui::Begin("シミュレーション", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		if (ImGui::Button(showSimulationWindow_ ? "シミュレーションツールを閉じる" : "シミュレーションツールを開く")) {
			showSimulationWindow_ = !showSimulationWindow_;
		}
		ImGui::End();

		// --- シミュレーション詳細ウィンドウ ---
		if (showSimulationWindow_) {
			ImGui::SetNextWindowSize(ImVec2(600.0f, 500.0f), ImGuiCond_Once);
			ImGui::Begin("シミュレーションツール", &showSimulationWindow_);
			
			const char* categories[] = { "プレイヤー", "ミサイル", "敵 & イベント", "パーティクル", "カメラ" };
			ImGui::Combo("カテゴリ", &currentSimulationTarget_, categories, IM_ARRAYSIZE(categories));
			ImGui::Separator();

			if (currentSimulationTarget_ == 0) {
				ImGui::Text("プレイヤー移動設定");
				if (player_) {
					auto mode = player_->GetCurrentMode();
					const char* modeName = (mode == PlayerMode::Fighter) ? "ファイター (1キー)" : 
										   (mode == PlayerMode::Gerwalk) ? "ガウォーク (2キー)" : "バトロイド (3キー)";
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "現在の形態: %s", modeName);
					
					PlayerModeParams p = player_->GetModeParams(mode);
					ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
					ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
					ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
					ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
					ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
					ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);
					player_->SetModeParams(mode, p);
				} else {
					ImGui::Text("プレイヤーが初期化されていません。");
				}
			}
			else if (currentSimulationTarget_ == 1) {
				DrawMissileSettingsUI();
			}
			else if (currentSimulationTarget_ == 2) {
				ImGui::Text("=== 敵の出現とルート ===");
				ImGui::Text("Lock-on: %s", lockedEnemy_ ? "LOCKED" : "NONE");
				ImGui::Text("Tab: lock target / X: unlock");
				ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

				if (ImGui::Button("敵を生成する")) {
					auto newEnemy = std::make_unique<Enemy>();
					newEnemy->Initialize({ newEnemyPos[0], newEnemyPos[1], newEnemyPos[2] });
					enemies_.push_back(std::move(newEnemy));
				}

				ImGui::Separator();
				ImGui::Text("=== 敵のリスト (総数: %d) ===", (int)enemies_.size());
				int index = 0;
				for (const auto& enemy : enemies_) {
					Vector3 pos = enemy->GetPosition();
					ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", index, pos.x, pos.y, pos.z);
					index++;
				}

				ImGui::Separator();
				ImGui::Text("イベントツール：増援設定");
				static int selectedTrigger = 0;
				static int selectedTarget = 0;
				static int eventDelay = 60;
				std::vector<const char*> enemyNames;
				for (const auto& sp : enemySpawns_) {
					enemyNames.push_back(sp.name.c_str());
				}
				if (!enemyNames.empty()) {
					if (selectedTrigger >= enemyNames.size()) selectedTrigger = 0;
					if (selectedTarget >= enemyNames.size()) selectedTarget = 0;
					ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
					ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
					ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);
					if (ImGui::Button("イベントを追加")) {
						enemyEventManager_.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
						enemySpawns_[selectedTarget].isInitialSpawn = false;
					}
					ImGui::SameLine();
					if (ImGui::Button("イベントを保存")) {
						enemyEventManager_.SaveEvents("resources/enemy_events.json");
					}
					for (size_t i = 0; i < enemyEventManager_.GetEvents().size(); ++i) {
						const auto& ev = enemyEventManager_.GetEvents()[i];
						ImGui::Text("[%d] %s が死んだら %d F後に %s が出現", (int)i, ev.triggerEnemyName.c_str(), ev.delayFrames, ev.targetEnemyName.c_str());
						ImGui::SameLine();
						if (ImGui::Button(("削除##" + std::to_string(i)).c_str())) {
							enemyEventManager_.RemoveEvent(i);
							break; 
						}
					}
				}

				ImGui::Separator();
				ImGui::Text("敵機ルート確認 (Editor Preview)");
				if (ImGui::Button("リセット")) ResetEditorPreview();
				ImGui::SameLine();
				if (ImGui::Button("再生")) isEditorPreviewPlaying_ = true;
				ImGui::SameLine();
				if (ImGui::Button("ストップ")) isEditorPreviewPlaying_ = false;
				ImGui::TextColored(isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "状態: %s", isEditorPreviewPlaying_ ? "再生中" : "停止中");
			}
			else if (currentSimulationTarget_ == 3) {
				ImGui::Text("=== GPU Particles ===");
				bool gpuChanged = false;
				if (auto *emitter = particleManager->GetEmitterSphere()) {
					if (ImGui::DragFloat3("位置", &emitter->translate.x, 0.01f)) gpuChanged = true;
					if (ImGui::DragFloat("射出半径", &emitter->radius, 0.01f)) gpuChanged = true;
					if (ImGui::DragInt("射出数", (int *) &emitter->count, 1, 0, 1000)) gpuChanged = true;
					if (ImGui::DragFloat("射出間隔", &emitter->frequency, 0.01f, 0.01f, 10.0f)) gpuChanged = true;
				}
				if (ImGui::Button("GPUパーティクルを再初期化") || gpuChanged) {
					particleManager->RequestGpuInitialize();
				}
				
				ImGui::Separator();
				ImGui::Text("=== Explosion Particles ===");
				if (explosionManager_) {
					auto& config = explosionManager_->GetConfig();
					ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
					ImGui::ColorEdit4("カラー", config.color);
					ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f);
					ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f);
					ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f);
					ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f);
					ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f);
					if (ImGui::Button("設定をJSONに保存")) explosionManager_->SaveToJson("resources/explosionConfig.json");
					ImGui::SameLine();
					if (ImGui::Button("設定をJSONから読込")) explosionManager_->LoadFromJson("resources/explosionConfig.json");
				}
			}
			else if (currentSimulationTarget_ == 4) {
				ImGui::Text("=== Camera Settings ===");
				if (isDebugCameraActive_) {
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[FREE CAM ACTIVE]");
					if (ImGui::Button("Switch to Player Camera")) SetDebugCameraActive(false);
					float moveSpd = debugFlyCamera_->GetMoveSpeed();
					float rotSpd  = debugFlyCamera_->GetRotateSpeed();
					float sens    = debugFlyCamera_->GetMouseSensitivity();
					float scroll  = debugFlyCamera_->GetScrollSpeed();
					float pan     = debugFlyCamera_->GetPanSpeed();
					if (ImGui::DragFloat("移動速度 (WASD)##fly", &moveSpd, 0.01f, 0.01f, 20.0f)) debugFlyCamera_->SetMoveSpeed(moveSpd);
					if (ImGui::DragFloat("回転感度 (マウス右)##fly", &sens, 0.0001f, 0.0001f, 0.05f, "%.4f")) debugFlyCamera_->SetMouseSensitivity(sens);
					if (ImGui::DragFloat("スクロール速度##fly", &scroll, 0.1f, 0.1f, 20.0f)) debugFlyCamera_->SetScrollSpeed(scroll);
					if (ImGui::DragFloat("パン速度 (中ボタン)##fly", &pan, 0.001f, 0.001f, 1.0f)) debugFlyCamera_->SetPanSpeed(pan);
					if (ImGui::DragFloat("回転速度 (キーボード)##fly",&rotSpd, 0.001f, 0.001f, 0.5f)) debugFlyCamera_->SetRotateSpeed(rotSpd);
					Vector3 flyPos = debugFlyCamera_->GetTranslate();
					float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
					if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
				} else {
					ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[PLAYER FOLLOW CAM]");
					if (ImGui::Button("Switch to Debug Camera")) SetDebugCameraActive(true);
					if (ImGui::Checkbox("Cinematic lock-on camera", &isCinematicLockOnCameraEnabled_)) isCinematicLockOnCameraInitialized_ = false;
					ImGui::Text("Cinematic: %s", (isCinematicLockOnCameraEnabled_ && lockedEnemy_) ? "ACTIVE" : "OFF");
					Vector3 camPos = camera->GetTranslate();
					float camPosArr[3] = { camPos.x, camPos.y, camPos.z };
					if (ImGui::DragFloat3("カメラ位置 (参考)##follow", camPosArr, 0.1f)) camera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
				}
			}

			ImGui::End();
		}

