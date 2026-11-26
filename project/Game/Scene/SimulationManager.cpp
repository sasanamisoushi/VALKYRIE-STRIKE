#include "SimulationManager.h"
#include "SimulationSceneJsonEditor.h"
#include "SimulationActionLibrary.h"
#include "SimulationBossEditor.h"
#include "BossSpawnController.h"
#include "EnemyFactory.h"
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

std::filesystem::path GetExecutablePackageZipPath() {
	const std::filesystem::path projectRoot = FindProjectRoot();
	return projectRoot.empty() ? std::filesystem::path{} :
		projectRoot / "generated" / "packages" / "ValkyrieStrike_Playable-Release.zip";
}

bool LaunchExecutablePackageCreation(bool skipBuild, bool includeDemoPlay, HANDLE& processHandle) {
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
	if (includeDemoPlay) {
		parameters += L" -IncludeDemoPlay";
	}
	SHELLEXECUTEINFOW executeInfo{};
	executeInfo.cbSize = sizeof(executeInfo);
	executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	executeInfo.lpVerb = L"open";
	executeInfo.lpFile = L"powershell.exe";
	executeInfo.lpParameters = parameters.c_str();
	executeInfo.lpDirectory = projectRoot.c_str();
	executeInfo.nShow = SW_SHOWNORMAL;
	if (!ShellExecuteExW(&executeInfo) || executeInfo.hProcess == nullptr) {
		return false;
	}

	processHandle = executeInfo.hProcess;
	return true;
}

void UpdateExecutablePackageStatus(HANDLE& processHandle, const std::filesystem::path& zipPath,
	std::string& packageMessage) {
	if (processHandle == nullptr || WaitForSingleObject(processHandle, 0) == WAIT_TIMEOUT) {
		return;
	}

	DWORD exitCode = 1;
	GetExitCodeProcess(processHandle, &exitCode);
	CloseHandle(processHandle);
	processHandle = nullptr;
	packageMessage = exitCode == 0 && std::filesystem::exists(zipPath)
		? "ZIP作成が完成しました！ generated/packages/ValkyrieStrike_Playable-Release.zip"
		: "ZIP作成に失敗しました。PowerShellのエラー内容を確認してください。";
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

SimulationSceneJsonContext SimulationManager::MakeSceneJsonContext() const {
	return {
		scene_->player_.get(), &scene_->enemies_, &scene_->obstacles_, &scene_->enemySpawns_,
		&scene_->stageReloadController_, &scene_->uiManager_->simulationSaveMessage_,
		[this]() { scene_->ReloadSceneJson(); },
	};
}

SimulationActionContext SimulationManager::MakeActionContext() const {
	return {
		scene_->isDebugCameraActive_, scene_->camera.get(), scene_->debugFlyCamera_.get(),
		scene_->player_.get(), &scene_->enemies_, &scene_->obstacles_, &scene_->enemySpawns_,
		&scene_->enemySpawnManager_, scene_->missileManager_.get(), scene_->enemyBulletManager_.get(),
		scene_->explosionManager_.get(),
		scene_->environmentRenderer_ ? scene_->environmentRenderer_->GetParticleManager() : nullptr,
		scene_->lockOnManager_.get(), &scene_->targetSelection_, &scene_->gameOverSequence_,
		{
			&scene_->missileNormalSpeed, &scene_->missileNormalScale, &scene_->missileNormalCollisionRadius,
			&scene_->missileNormalLifeTime, &scene_->missileSpeed, &scene_->missileAmpX, &scene_->missileAmpZ,
			&scene_->missileAmpY, &scene_->missileFreqY, &scene_->missileBaseY, &scene_->missileHomingStrength,
			&scene_->missileHomingScale, &scene_->missileHomingCollisionRadius, &scene_->missileTrailWidth,
			&scene_->missileLifeTime, &scene_->missileMuzzleOffset,
		},
		&scene_->uiManager_->simulationActionNames_, &scene_->uiManager_->selectedSimulationActionIndex_,
		&scene_->uiManager_->simulationActionMessage_,
		[this]() { scene_->ResetCinematicLockOnCamera(); },
	};
}

SimulationBossEditorContext SimulationManager::MakeBossEditorContext() const {
	return {
		&scene_->enemies_, &scene_->obstacles_, &scene_->bossEncounterState_,
		&scene_->gameOverSequence_, &scene_->isEditorPreviewPlaying_,
		scene_->uiManager_->bossBattlePosition_, &scene_->uiManager_->bossTuning_,
		&scene_->uiManager_->bossTuningMessage_,
		[this]() { scene_->ClearAllEnemyReferences(); },
	};
}

bool SimulationManager::SaveCurrentSimulationLayoutToSceneJson(const std::string &filePath) {
	return SimulationSceneJsonEditor::SaveCurrentLayout(filePath, MakeSceneJsonContext());
}

bool SimulationManager::AddEnemySpawnToSceneJson(
	const std::string &filePath, const std::string &name, const std::string &enemyType,
	const Vector3 &position, const Vector3 &rotation) {
	return SimulationSceneJsonEditor::AddEnemySpawn(
		filePath, name, enemyType, position, rotation, MakeSceneJsonContext());
}

bool SimulationManager::MoveEnemySpawnInSceneJson(const std::string &filePath, size_t sourceIndex, size_t targetIndex) {
	return SimulationSceneJsonEditor::MoveEnemySpawn(filePath, sourceIndex, targetIndex, MakeSceneJsonContext());
}

void SimulationManager::RefreshSimulationActionNames() {
	SimulationActionLibrary::RefreshActionNames(
		kSimulationActionsFilePath,
		scene_->uiManager_->simulationActionNames_,
		scene_->uiManager_->selectedSimulationActionIndex_);
}

bool SimulationManager::SaveNamedSimulationAction(const std::string &filePath, const std::string &actionName) {
	return SimulationActionLibrary::SaveNamedAction(filePath, actionName, MakeActionContext());
}

bool SimulationManager::ApplySimulationAction(const std::string &filePath, const std::string &actionName) {
	return SimulationActionLibrary::ApplyAction(filePath, actionName, MakeActionContext());
}

bool SimulationManager::SaveBossThrusterSettings(const std::string& filePath) {
	if (!scene_ || !scene_->uiManager_) return false;
	return SimulationBossEditor::SaveSettings(filePath, MakeBossEditorContext());
}

bool SimulationManager::LoadBossThrusterSettings(const std::string& filePath, bool verbose) {
	if (!scene_ || !scene_->uiManager_) return false;
	return SimulationBossEditor::LoadSettings(filePath, verbose, MakeBossEditorContext());
}

void SimulationManager::DrawBossBattleControls() {
	if (!scene_ || !scene_->uiManager_) return;
	SimulationBossEditor::DrawControls(MakeBossEditorContext());
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
		const char *categories[] = { "プレイヤー", "ミサイル", "敵 & イベント", "パーティクル", "カメラ", "アニメーション編集", "操作設定" };

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

			PlayerModeParams p = scene_->player_->GetModeParams(mode);
			ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
			ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
			ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
			ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
			ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);
			scene_->player_->SetModeParams(mode, p);

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
		ImGui::Text("Lock-on: %s", scene_->targetSelection_.HasLockedEnemy() ? "LOCKED" : "NONE");
		ImGui::Text("Tab: ターゲットロック / X: ロック解除 / F2: シミュレーションを閉じる");
		ImGui::Separator();
		ImGui::Text("=== ボス戦テスト / ボス調整 ===");
		ImGui::DragFloat3("ボス出現座標 (X,Y,Z)", scene_->uiManager_->bossBattlePosition_, 0.25f, -100.0f, 100.0f, "%.2f");
		ImGui::SameLine();
		if (ImGui::Button("座標を保存##simQuickSaveBossPos")) {
			SaveBossThrusterSettings("resources/boss_thruster_settings.json");
		}

		auto findActiveBoss = [&]() -> Boss* {
			for (const auto& enemy : scene_->enemies_) {
				if (auto* boss = dynamic_cast<Boss*>(enemy.get()); boss && !boss->IsDead()) return boss;
			}
			return nullptr;
		};

		Boss* activeBoss = findActiveBoss();
		if (ImGui::Button("この位置でボス戦を開始")) {
			scene_->ClearAllEnemyReferences();
			scene_->enemies_.clear();
			BossSpawnController bossSpawner;
			bossSpawner.Spawn(
				{ scene_->uiManager_->bossBattlePosition_[0], scene_->uiManager_->bossBattlePosition_[1], scene_->uiManager_->bossBattlePosition_[2] },
				&scene_->uiManager_->bossTuning_,
				scene_->obstacles_,
				scene_->enemies_);
			scene_->bossEncounterState_.MarkBossSpawned();
			scene_->gameOverSequence_.Reset();
			scene_->isEditorPreviewPlaying_ = true;
			scene_->uiManager_->bossTuningMessage_ = "ボス戦を開始しました。再生中のまま砲台とスラスターを調整できます。";
			activeBoss = findActiveBoss();
		}
		ImGui::SameLine();
		if (ImGui::Button("現在のボス位置を取得") && activeBoss) {
			const Vector3 position = activeBoss->GetPosition();
			scene_->uiManager_->bossBattlePosition_[0] = position.x;
			scene_->uiManager_->bossBattlePosition_[1] = position.y;
			scene_->uiManager_->bossBattlePosition_[2] = position.z;
		}

		if (activeBoss) {
			if (ImGui::Button("設定した座標へボスを移動")) {
				activeBoss->SetPosition({ scene_->uiManager_->bossBattlePosition_[0], scene_->uiManager_->bossBattlePosition_[1], scene_->uiManager_->bossBattlePosition_[2] });
				activeBoss->UpdateModel();
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.45f, 1.0f), "ボス出現中  HP: %d/%d", activeBoss->GetHP(), activeBoss->GetMaxHP());
		} else {
			ImGui::TextDisabled("ボスはまだ出現していません。開始ボタンで設定を反映します。");
		}

		bool bossTuningChanged = false;
		if (ImGui::CollapsingHeader("砲台の回転・スライド", ImGuiTreeNodeFlags_DefaultOpen)) {
			Boss::Tuning& tuning = scene_->uiManager_->bossTuning_;
			bossTuningChanged |= ImGui::Checkbox("主砲の横追尾 (ヨー)", &tuning.mainTurretYawTracking);
			bossTuningChanged |= ImGui::Checkbox("主砲の縦追尾 (ピッチ)", &tuning.mainTurretPitchTracking);
			bossTuningChanged |= ImGui::SliderFloat("主砲 横追尾の速さ", &tuning.mainTurretYawFollow, 0.0f, 0.35f, "%.3f");
			bossTuningChanged |= ImGui::SliderFloat("主砲 縦追尾の速さ", &tuning.mainTurretPitchFollow, 0.0f, 0.35f, "%.3f");
			bossTuningChanged |= ImGui::SliderFloat("主砲 最大仰角", &tuning.mainTurretMaxPitch, 0.0f, 1.20f, "%.2f rad");

			ImGui::TextDisabled("艦首の左右2砲台: 横回転なし、ピッチ（上下回転）だけで照準します。中央砲・主砲は従来どおり横回転します。");
		}

		if (ImGui::CollapsingHeader("後部スラスター", ImGuiTreeNodeFlags_DefaultOpen)) {
			Boss::Tuning& tuning = scene_->uiManager_->bossTuning_;
			bossTuningChanged |= ImGui::SliderInt("スラスター数", &tuning.rearThrusterCount, 0, 12);
			bossTuningChanged |= ImGui::DragFloat3("スラスター位置補正", &tuning.rearThrusterOffset.x, 0.02f, -2.0f, 2.0f, "%.2f");
			bossTuningChanged |= ImGui::SliderFloat("スラスター間隔", &tuning.rearThrusterSpacing, 0.05f, 1.20f, "%.2f");
			bossTuningChanged |= ImGui::SliderFloat("炎の太さ", &tuning.rearThrusterRadius, 0.02f, 1.20f, "%.2f");
			bossTuningChanged |= ImGui::SliderFloat("炎の長さ", &tuning.rearThrusterLength, 0.05f, 6.00f, "%.2f");
			bossTuningChanged |= ImGui::SliderFloat("炎の明るさ", &tuning.rearThrusterBrightness, 0.0f, 2.50f, "%.2f");
			bossTuningChanged |= ImGui::SliderFloat("根元フレア半径", &tuning.rearThrusterFlareRadius, 0.02f, 1.50f, "%.2f");
			ImGui::TextDisabled("追加したスラスターは、後部の矩形パネル中心を基準に横方向へ等間隔で並びます。");
		}

		if (ImGui::CollapsingHeader("側面シールド", ImGuiTreeNodeFlags_DefaultOpen)) {
			Boss::Tuning& tuning = scene_->uiManager_->bossTuning_;
			bossTuningChanged |= ImGui::SliderFloat("シールド不透明度", &tuning.sideShieldOpacity, 0.05f, 1.0f, "%.2f");
			ImGui::TextDisabled("変更は出現中のボスへ即時反映され、保存ボタンで次回以降も維持されます。");
		}

		if (bossTuningChanged) {
			if (activeBoss) {
				activeBoss->SetTuning(scene_->uiManager_->bossTuning_);
				activeBoss->UpdateModel();
				scene_->uiManager_->bossTuningMessage_ = "出現中のボスへ設定を反映しました。";
			} else {
				scene_->uiManager_->bossTuningMessage_ = "設定しました。次にボス戦を開始したとき反映されます。";
			}
		}
		if (!scene_->uiManager_->bossTuningMessage_.empty()) ImGui::TextWrapped("%s", scene_->uiManager_->bossTuningMessage_.c_str());
		if (ImGui::Button("ボスの位置・設定を保存##bossSim")) {
			SaveBossThrusterSettings("resources/boss_thruster_settings.json");
		}
		ImGui::SameLine();
		if (ImGui::Button("保存したボスの位置・設定を読み込む##bossSim")) {
			LoadBossThrusterSettings("resources/boss_thruster_settings.json");
		}
		ImGui::TextDisabled("保存先: resources/boss_thruster_settings.json");

		ImGui::DragFloat3("出現座標 (X,Y,Z)", scene_->newEnemyPos, 1.0f);

		if (ImGui::Button("敵を生成する！")) {
			EnemySpawnData spawnData{};
			spawnData.position = { scene_->newEnemyPos[0], scene_->newEnemyPos[1], scene_->newEnemyPos[2] };
			EnemyFactory factory;
			if (auto newEnemy = factory.Create(spawnData, (std::numeric_limits<size_t>::max)(), scene_->obstacles_)) {
				scene_->enemies_.push_back(std::move(newEnemy));
			}
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
			const char *destructionParticleTypes[] = {
				"炎 → 煙（推奨）", "火球", "煙雲", "火花"
			};
			config.destructionParticleType = (std::clamp)(config.destructionParticleType, 0, IM_ARRAYSIZE(destructionParticleTypes) - 1);
			ImGui::Combo("撃破パーティクル", &config.destructionParticleType,
				destructionParticleTypes, IM_ARRAYSIZE(destructionParticleTypes));
			ImGui::TextDisabled("撃破時に使うパーティクルの種類を選択します。");
			ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
			ImGui::ColorEdit4("カラー", config.color);
			if (config.destructionParticleType == static_cast<int>(ExplosionManager::DestructionParticleType::FlameToSmoke)) {
				ImGui::ColorEdit4("煙カラー", config.smokeColor);
				ImGui::DragFloat("煙への変化開始", &config.flameToSmokeTransition, 0.01f, 0.0f, 0.95f);
				ImGui::DragFloat("最終スケール倍率", &config.flameToSmokeScale, 0.05f, 1.0f, 8.0f);
			}
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
			bool cinematicLockOnEnabled = scene_->IsCinematicLockOnCameraEnabled();
			if (ImGui::Checkbox("Cinematic lock-on scene_->camera", &cinematicLockOnEnabled)) {
				scene_->SetCinematicLockOnCameraEnabled(cinematicLockOnEnabled);
			}
			ImGui::Text("Cinematic: %s", (scene_->IsCinematicLockOnCameraEnabled() && scene_->targetSelection_.HasLockedEnemy()) ? "ACTIVE" : "OFF");
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
		static HANDLE packageProcess = nullptr;
		static bool includeDemoPlay = false;
		const bool projectFound = !FindProjectRoot().empty();
		const bool runningRelease = IsCurrentExecutableReleaseBuild();
		const std::filesystem::path zipPath = GetExecutablePackageZipPath();
		UpdateExecutablePackageStatus(packageProcess, zipPath, packageMessage);
		const bool packageCreationRunning = packageProcess != nullptr;
		ImGui::Text("実行ファイル生成");
		ImGui::TextWrapped("Release版のValkyrieStrike.exe、実行用DLL、resourcesをまとめ、配布用ZIPを作成します。");
		ImGui::Separator();
		ImGui::BeginDisabled(packageCreationRunning);
		ImGui::Checkbox("デモプレイを含める", &includeDemoPlay);
		ImGui::EndDisabled();
		ImGui::TextDisabled("ONにするとデモ専用EXEを同梱し、タイトルのTABでクリア演出デモを再生できます。");
		ImGui::Separator();
		if (!projectFound) {
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "プロジェクトフォルダー内から起動した場合のみ使用できます。");
		} else if (packageCreationRunning) {
			ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f), "ZIP作成中です。完了までお待ちください。");
		} else {
			if (runningRelease) {
				ImGui::TextDisabled("現在Release版を実行中のため、実行ファイルを上書きするビルドはできません。");
				if (ImGui::Button("現在のRelease版をZIP作成")) {
					packageMessage = LaunchExecutablePackageCreation(true, includeDemoPlay, packageProcess)
						? "ZIP作成中です。完了を確認しています。"
						: "ZIP作成を開始できませんでした。";
				}
			} else if (ImGui::Button("ReleaseビルドしてZIP作成")) {
				packageMessage = LaunchExecutablePackageCreation(false, includeDemoPlay, packageProcess)
					? "ReleaseビルドとZIP作成中です。完了を確認しています。"
					: "実行ファイル生成を開始できませんでした。";
			}
			ImGui::TextDisabled("出力先: generated/packages/ValkyrieStrike_Playable-Release.zip");
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
	animationEditorPanel_.RefreshActionAnimationsList();
}

void SimulationManager::DrawAnimationEditorUI() {
	animationEditorPanel_.DrawUI(scene_ ? scene_->player_.get() : nullptr, scene_ && scene_->isEditorPreviewPlaying_);
}

void SimulationManager::AddBoneRotationFromDrag(const std::string& boneName, float deltaX, float deltaY) {
	animationEditorPanel_.AddBoneRotationFromDrag(boneName, deltaX, deltaY);
}

void SimulationManager::AddBoneTranslationFromDrag(const std::string& boneName, float deltaX, float deltaY) {
	animationEditorPanel_.AddBoneTranslationFromDrag(boneName, deltaX, deltaY);
}

void SimulationManager::UpdateShortcuts() {
	const bool isAnimationTabActive = scene_ && scene_->uiManager_ && scene_->uiManager_->currentSimulationTarget_ == 5;
	animationEditorPanel_.UpdateShortcuts(isAnimationTabActive, scene_ ? scene_->player_.get() : nullptr);
}

void SimulationManager::PushUndo() {
	animationEditorPanel_.PushUndo();
}

void SimulationManager::Undo() {
	animationEditorPanel_.Undo(scene_ ? scene_->player_.get() : nullptr);
}

void SimulationManager::Redo() {
	animationEditorPanel_.Redo(scene_ ? scene_->player_.get() : nullptr);
}

void SimulationManager::DeleteSelectedBonesKeyframes() {
	animationEditorPanel_.DeleteSelectedBonesKeyframes();
}
