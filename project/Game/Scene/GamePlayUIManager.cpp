#include "GamePlayUIManager.h"
#include "GamePlayScene.h"
#include "GamePlaySceneHelpers.h"
#include "engine/Debug/ImGuiManager.h"
#include <externals/imgui/imgui.h>
#include "SimulationManager.h"
#include "MissilePresetManager.h"
#include "LockOnManager.h"
#include "engine/Utility/StageValidation.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Input/Input.h"
#include "engine/Scene/SceneManager.h"
#include "engine/math/MyMath.h"
#include "externals/json.hpp"
#include <Windows.h>
#include <filesystem>
#include <shellapi.h>
#include <vector>

#if defined(ENABLE_IMGUI)
namespace {
	std::string gBlenderLaunchMessage;

	std::filesystem::path FindStageBlendPath() {
		const std::filesystem::path relativeBlendPath = L"resources\\level_editor\\stage.blend";
		std::vector<std::filesystem::path> searchStarts;

		try {
			searchStarts.push_back(std::filesystem::current_path());
		} catch (...) {
		}

		wchar_t modulePath[MAX_PATH] = {};
		const DWORD modulePathLength = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (modulePathLength > 0) {
			searchStarts.push_back(std::filesystem::path(modulePath).parent_path());
		}

		for (const std::filesystem::path &startPath : searchStarts) {
			std::filesystem::path cursor = startPath;
			while (!cursor.empty()) {
				const std::filesystem::path candidates[] = {
					cursor / relativeBlendPath,
					cursor / L"project" / relativeBlendPath
				};

				for (const std::filesystem::path &candidate : candidates) {
					if (std::filesystem::exists(candidate)) {
						return std::filesystem::absolute(candidate);
					}
				}

				if (cursor == cursor.root_path()) {
					break;
				}
				cursor = cursor.parent_path();
			}
		}

		return {};
	}

	bool OpenStageBlendFile() {
		const std::filesystem::path blendPath = FindStageBlendPath();
		if (blendPath.empty()) {
			gBlenderLaunchMessage = "stage.blend が見つかりません。";
			OutputDebugStringA("[OpenBlender] resources/level_editor/stage.blend not found.\n");
			return false;
		}

		HINSTANCE result = ShellExecuteW(
			nullptr,
			L"open",
			blendPath.c_str(),
			nullptr,
			blendPath.parent_path().c_str(),
			SW_SHOWNORMAL);

		if (reinterpret_cast<intptr_t>(result) <= 32) {
			result = ShellExecuteW(
				nullptr,
				L"open",
				L"blender.exe",
				blendPath.c_str(),
				blendPath.parent_path().c_str(),
				SW_SHOWNORMAL);
		}

		if (reinterpret_cast<intptr_t>(result) <= 32) {
			gBlenderLaunchMessage = "Blender を起動できませんでした。関連付けまたは PATH を確認してください。";
			OutputDebugStringW((L"[OpenBlender] Failed: " + blendPath.wstring() + L"\n").c_str());
			return false;
		}

		gBlenderLaunchMessage = "Blender を起動しました。";
		OutputDebugStringW((L"[OpenBlender] Opened: " + blendPath.wstring() + L"\n").c_str());
		return true;
	}

	std::filesystem::path FindProjectRoot() {
		wchar_t modulePath[MAX_PATH] = {};
		if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0) {
			return {};
		}

		std::filesystem::path cursor = std::filesystem::path(modulePath).parent_path();
		while (!cursor.empty()) {
			if (std::filesystem::exists(cursor / "project" / "CG2.sln") &&
				std::filesystem::exists(cursor / "tools" / "package_release.ps1")) {
				return cursor;
			}
			const std::filesystem::path parent = cursor.parent_path();
			if (parent == cursor) {
				break;
			}
			cursor = parent;
		}
		return {};
	}

	bool CreateExecutablePackage(bool skipBuild) {
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
		return std::filesystem::path(modulePath).lexically_normal() ==
			(projectRoot / "generated" / "outputs" / "Release" / "CG2.exe").lexically_normal();
	}
}
#endif

#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
	bool gShowStageValidationWindow = true;
	bool gShowStageValidationLabels = true;

	ImVec4 ValidationLevelColor(const std::string &level) {
		if (level == "ERROR") {
			return ImVec4(1.0f, 0.25f, 0.2f, 1.0f);
		}
		if (level == "WARNING") {
			return ImVec4(1.0f, 0.75f, 0.2f, 1.0f);
		}
		return ImVec4(0.35f, 1.0f, 0.45f, 1.0f);
	}

	const char *ValidationLevelText(const std::string &level) {
		if (level == "ERROR") {
			return "ERROR";
		}
		if (level == "WARNING") {
			return "WARNING";
		}
		return "OK";
	}

	void DrawValidationMessages(const char *title, const std::vector<std::string> &messages, const ImVec4 &color) {
		ImGui::TextColored(color, "%s: %d", title, static_cast<int>(messages.size()));
		for (const std::string &message : messages) {
			ImGui::BulletText("%s", message.c_str());
		}
	}

	void DrawStageValidationWindow(const StageValidation::Report &report) {
		ImGui::Begin("Level Validation");
		ImGui::Text("Source: %s", report.source.c_str());

		DrawValidationMessages("Errors", report.errors, ValidationLevelColor("ERROR"));
		DrawValidationMessages("Warnings", report.warnings, ValidationLevelColor("WARNING"));

		ImGui::Separator();
		ImGui::Text("Check Items");
		for (const StageValidation::CheckItemResult &item : report.checkItems) {
			ImGui::TextColored(ValidationLevelColor(item.level), "%s", ValidationLevelText(item.level));
			ImGui::SameLine(90.0f);
			if (item.detail.empty()) {
				ImGui::TextWrapped("%s", item.label.c_str());
			} else {
				ImGui::TextWrapped("%s: %s", item.label.c_str(), item.detail.c_str());
			}
		}

		ImGui::End();
	}

	struct ValidationLabelRect {
		ImVec2 min;
		ImVec2 max;
	};

	float ClampValidationFloat(float value, float minValue, float maxValue) {
		if (maxValue < minValue) {
			return minValue;
		}
		if (value < minValue) {
			return minValue;
		}
		if (value > maxValue) {
			return maxValue;
		}
		return value;
	}

	ValidationLabelRect MakeValidationLabelRect(const ImVec2 &textPos, const ImVec2 &textSize, float padding) {
		return {
			ImVec2(textPos.x - padding, textPos.y - padding),
			ImVec2(textPos.x + textSize.x + padding, textPos.y + textSize.y + padding),
		};
	}

	bool ValidationRectsOverlap(const ValidationLabelRect &lhs, const ValidationLabelRect &rhs) {
		return lhs.min.x < rhs.max.x && lhs.max.x > rhs.min.x &&
			lhs.min.y < rhs.max.y && lhs.max.y > rhs.min.y;
	}

	bool ValidationRectOverlapsAny(const ValidationLabelRect &rect, const std::vector<ValidationLabelRect> &usedRects) {
		for (const ValidationLabelRect &usedRect : usedRects) {
			if (ValidationRectsOverlap(rect, usedRect)) {
				return true;
			}
		}
		return false;
	}

	void ClampValidationLabelToGameView(
		ImVec2 &textPos,
		const ImVec2 &textSize,
		float padding,
		float minX,
		float minY,
		float maxX,
		float maxY) {

		textPos.x = ClampValidationFloat(textPos.x, minX + padding, maxX - textSize.x - padding);
		textPos.y = ClampValidationFloat(textPos.y, minY + padding, maxY - textSize.y - padding);
	}

	ImVec2 ResolveValidationLabelPosition(
		const ImVec2 &anchor,
		const ImVec2 &textSize,
		float padding,
		float minX,
		float minY,
		float maxX,
		float maxY,
		std::vector<ValidationLabelRect> &usedRects) {

		const float rectWidth = textSize.x + padding * 2.0f;
		const float rectHeight = textSize.y + padding * 2.0f;
		const float verticalStep = rectHeight + 4.0f;
		const float horizontalStep = rectWidth * 0.55f;
		const int horizontalSlots[] = { 0, -1, 1, -2, 2 };

		for (int stackIndex = 0; stackIndex < 10; ++stackIndex) {
			for (int side = 0; side < 2; ++side) {
				for (int horizontalSlot : horizontalSlots) {
					const float x = anchor.x - textSize.x * 0.5f + horizontalStep * static_cast<float>(horizontalSlot);
					const float y = (side == 0)
						? anchor.y - textSize.y - 22.0f - verticalStep * static_cast<float>(stackIndex)
						: anchor.y + 16.0f + verticalStep * static_cast<float>(stackIndex);
					ImVec2 candidate(x, y);
					ClampValidationLabelToGameView(candidate, textSize, padding, minX, minY, maxX, maxY);

					const ValidationLabelRect rect = MakeValidationLabelRect(candidate, textSize, padding);
					if (!ValidationRectOverlapsAny(rect, usedRects)) {
						usedRects.push_back(rect);
						return candidate;
					}
				}
			}
		}

		ImVec2 fallback(anchor.x - textSize.x * 0.5f, anchor.y - textSize.y - 22.0f);
		ClampValidationLabelToGameView(fallback, textSize, padding, minX, minY, maxX, maxY);
		usedRects.push_back(MakeValidationLabelRect(fallback, textSize, padding));
		return fallback;
	}

	void DrawStageValidationOverlay(const StageValidation::Report &report, const Matrix4x4 &viewProjectionMatrix) {
		if (!report.HasMarkers()) {
			return;
		}

		float minX = 0.0f;
		float minY = 0.0f;
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (!FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY)) {
			return;
		}

		const float width = maxX - minX;
		const float height = maxY - minY;
		if (!ImGui::GetCurrentContext()) {
			return;
		}
		ImDrawList *drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
		std::vector<ValidationLabelRect> usedRects;

		for (const StageValidation::ValidationMarker &marker : report.markers) {
			Vector3 worldPosition = {
				static_cast<float>(marker.x),
				static_cast<float>(marker.y) + 1.2f,
				static_cast<float>(marker.z),
			};
			Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
			if (screenPosition.z < 0.0f || screenPosition.z > 1.0f) {
				continue;
			}
			if (screenPosition.x < 0.0f || screenPosition.x > width || screenPosition.y < 0.0f || screenPosition.y > height) {
				continue;
			}

			const bool isError = marker.level == "ERROR";
			const std::string text = std::string(isError ? "ERROR: " : "WARNING: ") + marker.message;
			const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
			const float padding = 4.0f;
			const ImVec2 anchor(minX + screenPosition.x, minY + screenPosition.y);
			const ImVec2 textPos = ResolveValidationLabelPosition(anchor, textSize, padding, minX, minY, maxX, maxY, usedRects);
			const ValidationLabelRect rect = MakeValidationLabelRect(textPos, textSize, padding);

			const ImU32 fillColor = isError ? IM_COL32(90, 16, 16, 220) : IM_COL32(92, 63, 0, 220);
			const ImU32 borderColor = isError ? IM_COL32(255, 75, 55, 255) : IM_COL32(255, 200, 45, 255);
			drawList->AddRectFilled(rect.min, rect.max, fillColor, 3.0f);
			drawList->AddRect(rect.min, rect.max, borderColor, 3.0f);
			drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text.c_str());
		}
	}
#endif


GamePlayUIManager::GamePlayUIManager(GamePlayScene* scene) : scene_(scene) {
}

void GamePlayUIManager::Initialize() {
}

void GamePlayUIManager::DrawEditorToolbar() {
#ifdef ENABLE_IMGUI
	if (!ImGui::BeginMainMenuBar()) {
		return;
	}

	if (ImGui::Button("再生")) {
		// 毎回、編集時の初期配置に戻してからゲームループを開始する。
		scene_->ResetEditorPreview();
		scene_->isEditorPreviewPlaying_ = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("編集に戻す")) {
		scene_->ResetEditorPreview();
	}

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	static int selectedSceneIndex = 1;
	const char *sceneLabels[] = { "タイトル", "ゲーム", "シミュレーション" };
	const char *sceneNames[] = { "TITLE", "GAMEPLAY", "SIMULATION" };
	ImGui::SetNextItemWidth(130.0f);
	ImGui::Combo("##切替先シーン", &selectedSceneIndex, sceneLabels, IM_ARRAYSIZE(sceneLabels));
	ImGui::SameLine();
	if (ImGui::Button("シーン切替")) {
		SceneManager::GetInstance()->ChangeScene(sceneNames[selectedSceneIndex]);
	}

	ImGui::SameLine();
	ImGui::TextDisabled(scene_->isEditorPreviewPlaying_ ? "再生中" : "編集モード（停止中）");
	ImGui::EndMainMenuBar();
#endif
}

void GamePlayUIManager::UpdateUI() {
    if (!scene_) return;
#define enemyEventManager_ scene_->enemyEventManager_
#define lockedEnemy_ scene_->lockedEnemy_
#define aimAssistEnemy_ scene_->aimAssistEnemy_
#define isMultiLockCharging_ scene_->isMultiLockCharging_
#define multiLockTargets_ scene_->multiLockTargets_

#define mode_ scene_->mode_
#define camera scene_->camera
#define sprite scene_->sprite
#define groundModel scene_->groundModel
#define myShere scene_->myShere
#define skybox scene_->skybox
#define aimCursorSprite_ scene_->aimCursorSprite_
#define lockOnReticleSprite_ scene_->lockOnReticleSprite_
#define boundaryAlertObject_ scene_->boundaryAlertObject_
#define ceilingBoundaryAlertObject_ scene_->ceilingBoundaryAlertObject_
#define particleManager scene_->environmentRenderer_->GetParticleManager()
#define particleEmitter scene_->environmentRenderer_->GetParticleEmitter()
#define soundData1 scene_->soundData1
#define soundData2 scene_->soundData2
#define boundaryAlertPlane_ scene_->boundaryAlertPlane_
#define myBox scene_->myBox
#define myRing scene_->environmentRenderer_->myRing_
#define myPartialRing scene_->environmentRenderer_->myPartialRing_
#define myCylinder scene_->environmentRenderer_->myCylinder_
#define myModelObject scene_->myModelObject
#define showNormalRing scene_->environmentRenderer_->showNormalRing_
#define showPartialRing scene_->environmentRenderer_->showPartialRing_
#define showCylinder scene_->environmentRenderer_->showCylinder_
#define prSubdivision scene_->environmentRenderer_->prSubdivision_
#define prOuterRadius scene_->environmentRenderer_->prOuterRadius_
#define prInnerRadius scene_->environmentRenderer_->prInnerRadius_
#define prIsUvHorizontal scene_->environmentRenderer_->prIsUvHorizontal_
#define prInnerColor scene_->environmentRenderer_->prInnerColor_
#define prOuterColor scene_->environmentRenderer_->prOuterColor_
#define prStartAngle scene_->environmentRenderer_->prStartAngle_
#define prEndAngle scene_->environmentRenderer_->prEndAngle_
#define prFadeAngle scene_->environmentRenderer_->prFadeAngle_
#define cylinderPos scene_->environmentRenderer_->cylinderPos_
#define cylinderScale scene_->environmentRenderer_->cylinderScale_
#define cylinderUVOffset scene_->environmentRenderer_->cylinderUVOffset_
#define cylinderUVScrollSpeed scene_->environmentRenderer_->cylinderUVScrollSpeed_
#define cylinderAlphaReference scene_->environmentRenderer_->cylinderAlphaReference_
#define cylinderSubdivision scene_->environmentRenderer_->cylinderSubdivision_
#define cylinderVerticalSubdivision scene_->environmentRenderer_->cylinderVerticalSubdivision_
#define cylinderTopRadiusX scene_->environmentRenderer_->cylinderTopRadiusX_
#define cylinderTopRadiusZ scene_->environmentRenderer_->cylinderTopRadiusZ_
#define cylinderBottomRadiusX scene_->environmentRenderer_->cylinderBottomRadiusX_
#define cylinderBottomRadiusZ scene_->environmentRenderer_->cylinderBottomRadiusZ_
#define cylinderHeight scene_->environmentRenderer_->cylinderHeight_
#define cylinderTopColor scene_->environmentRenderer_->cylinderTopColor_
#define cylinderBottomColor scene_->environmentRenderer_->cylinderBottomColor_
#define cylinderStartAngle scene_->environmentRenderer_->cylinderStartAngle_
#define cylinderEndAngle scene_->environmentRenderer_->cylinderEndAngle_
#define cylinderIsUvFlipped scene_->environmentRenderer_->cylinderIsUvFlipped_
#define animationData scene_->animationData
#define animationTime scene_->animationTime
#define playAnimation scene_->playAnimation
#define skeleton scene_->skeleton
#define showBones scene_->showBones
#define showPlane scene_->showPlane
#define showSphere scene_->showSphere
#define showBox scene_->showBox
#define showTrail scene_->showTrail
#define showSkybox scene_->environmentRenderer_->showSkybox_
#define showSprite scene_->showSprite
#define skeletonLinesModel scene_->skeletonLinesModel
#define skeletonLinesObject scene_->skeletonLinesObject
#define debugColliderLinesObject scene_->debugColliderLinesObject
#define showDebugColliders scene_->showDebugColliders
#define debugFlyCamera_ scene_->debugFlyCamera_
#define isDebugCameraActive_ scene_->isDebugCameraActive_
#define isEditorPreviewPlaying_ scene_->isEditorPreviewPlaying_
#define isCinematicLockOnCameraEnabled_ scene_->isCinematicLockOnCameraEnabled_
#define isCinematicLockOnCameraInitialized_ scene_->isCinematicLockOnCameraInitialized_
#define cinematicLockOnCameraPosition_ scene_->cinematicLockOnCameraPosition_
#define cinematicLockOnCameraRotation_ scene_->cinematicLockOnCameraRotation_
#define cinematicLockOnCameraFocus_ scene_->cinematicLockOnCameraFocus_
#define cinematicLockOnCameraBackDirection_ scene_->cinematicLockOnCameraBackDirection_
#define cinematicLockOnCameraSideSign_ scene_->cinematicLockOnCameraSideSign_
#define cinematicLockOnCameraSeparation_ scene_->cinematicLockOnCameraSeparation_
#define showParticles scene_->environmentRenderer_->showParticles_
#define showModel scene_->showModel
#define enableSkinning scene_->enableSkinning
#define modelScale scene_->modelScale
#define currentAnimationIndex scene_->currentAnimationIndex
#define missileTrail scene_->missileTrail
#define trailObject scene_->trailObject
#define missileNormalSpeed scene_->missileNormalSpeed
#define missileNormalScale scene_->missileNormalScale
#define missileNormalCollisionRadius scene_->missileNormalCollisionRadius
#define missileNormalLifeTime scene_->missileNormalLifeTime
#define missileSpeed scene_->missileSpeed
#define missileAmpX scene_->missileAmpX
#define missileAmpZ scene_->missileAmpZ
#define missileAmpY scene_->missileAmpY
#define missileFreqY scene_->missileFreqY
#define missileBaseY scene_->missileBaseY
#define missileHomingStrength scene_->missileHomingStrength
#define missileHomingScale scene_->missileHomingScale
#define missileHomingCollisionRadius scene_->missileHomingCollisionRadius
#define missileTrailWidth scene_->missileTrailWidth
#define missileLifeTime scene_->missileLifeTime
#define missileMuzzleOffset scene_->missileMuzzleOffset
#define player_ scene_->player_
#define missileManager_ scene_->missileManager_
#define enemies_ scene_->enemies_
#define enemyBulletManager_ scene_->enemyBulletManager_
#define enemySpawns_ scene_->enemySpawns_
#define enemyRespawnTimers_ scene_->enemyRespawnTimers_
#define obstacles_ scene_->obstacles_
#define newEnemyPos scene_->newEnemyPos
#define explosionManager_ scene_->explosionManager_
#define isGameOver_ scene_->isGameOver_
#define gameOverTimer_ scene_->gameOverTimer_
#define simulationManager_ scene_->simulationManager_
#define missilePresetManager_ scene_->missilePresetManager_
#define lockOnManager_ scene_->lockOnManager_
#define cameraManager_ scene_->cameraManager_
#define levelManager_ scene_->levelManager_
#define uiManager_ scene_->uiManager_
#define ammoManager_ scene_->ammoManager_
#define lastJsonWriteTime_ scene_->lastJsonWriteTime_
#define aimAssistEnemy_ scene_->aimAssistEnemy_
#define isMultiLockCharging_ scene_->isMultiLockCharging_
#define multiLockChargeFrames_ scene_->multiLockChargeFrames_
#define IsSimulationMode scene_->IsSimulationMode
#define SetDebugCameraActive scene_->SetDebugCameraActive
#define ReloadSceneJson scene_->ReloadSceneJson
#define ResetEditorPreview scene_->ResetEditorPreview
#define MakeMissileTuning scene_->MakeMissileTuning
#define SpawnEnemyFromSpawnPoint scene_->SpawnEnemyFromSpawnPoint
#define IsEnemySpawnPointActive scene_->IsEnemySpawnPointActive
#define ScheduleEnemySpawn scene_->ScheduleEnemySpawn
#define TriggerEnemyReinforcements scene_->TriggerEnemyReinforcements
#define UpdateEnemyRespawns scene_->UpdateEnemyRespawns
#define HasPendingEnemySpawns scene_->HasPendingEnemySpawns
#define UpdateCinematicLockOnCamera scene_->UpdateCinematicLockOnCamera


#ifdef ENABLE_IMGUI
	if (ImGuiManager::IsVisible()) {
		if (IsSimulationMode()) {
			simulationManager_->DrawSimulationScreenUI();
			return;
		}
		DrawEditorToolbar();

		ImGui::Begin("Simulation");
		ImGui::Text("シミュレーション設定");
		ImGui::TextWrapped("保存済み設定の読み込みはここで行えます。細かい保存や確認は専用画面を開いてください。");
		if (ImGui::Button("シミュレーション画面を開く (F2)")) {
			LaunchSimulationExecutable();
		}
		DrawGameplayActionControls();
		DrawAmmoSettingsUI();
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
					
					PlayerModeParams& p = player_->GetModeParams(mode);
					ImGui::SliderFloat("最大移動速度", &p.maxMoveSpeed, 0.01f, 1.0f);
					ImGui::SliderFloat("移動加速度", &p.moveAcceleration, 0.001f, 0.1f);
					ImGui::SliderFloat("移動減衰", &p.moveDamping, 0.1f, 1.0f);
					ImGui::SliderFloat("ピッチ回転速度", &p.pitchSpeed, 0.001f, 0.1f);
					ImGui::SliderFloat("ヨー回転速度", &p.yawSpeed, 0.001f, 0.1f);
					ImGui::SliderFloat("ロール回転速度", &p.rollSpeed, 0.001f, 0.1f);
				} else {
					ImGui::Text("プレイヤーが初期化されていません。");
				}
			}
			else if (currentSimulationTarget_ == 1) {
				missilePresetManager_->DrawMissileSettingsUI();
			}
			else if (currentSimulationTarget_ == 2) {
				ImGui::Text("=== 敵の出現とルート ===");
				ImGui::Text("Lock-on: %s", lockedEnemy_ ? "LOCKED" : "NONE");
				ImGui::Text("Tab: lock target / X: unlock");
				ImGui::DragFloat3("出現座標 (X,Y,Z)", newEnemyPos, 1.0f);

				if (ImGui::Button("敵を生成する！")) {
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
				ImGui::Text("イベントツール（増援設定）");
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

		// --- オリジナルUI（今まで出していたImGui関連） ---
		//開発用UIの処理
		}
		// 設定一覧と設定内容は、見本のように独立したドック枠として表示する。
		const char *engineSettingsCategories[] = {
			"表示・エフェクト",
			"カメラ",
			"敵・障害物",
			"敵撃破パーティクル",
			"レベルエディタ",
			"操作設定",
			"実行ファイル生成",
			"スラスター位置",
		};

		ImGui::SetNextWindowSize(ImVec2(200.0f, 620.0f), ImGuiCond_Once);
		ImGui::Begin("設定一覧");
		ImGui::Separator();
		for (int categoryIndex = 0; categoryIndex < IM_ARRAYSIZE(engineSettingsCategories); ++categoryIndex) {
			const bool isSelected = currentEngineSettingsTarget_ == categoryIndex;
			if (ImGui::Selectable(engineSettingsCategories[categoryIndex], isSelected, 0, ImVec2(0.0f, 32.0f))) {
				currentEngineSettingsTarget_ = categoryIndex;
			}
		}
		ImGui::End();

		ImGui::SetNextWindowSize(ImVec2(500.0f, 620.0f), ImGuiCond_Once);
		ImGui::Begin("設定内容");
		ImGui::Text("%s の設定", engineSettingsCategories[currentEngineSettingsTarget_]);
		ImGui::Separator();

		if (currentEngineSettingsTarget_ == 0) {

		ImGui::Text("表示設定");
		ImGui::Checkbox("スカイボックスを表示", &showSkybox);
		ImGui::Checkbox("平面を表示", &showPlane);
		ImGui::Checkbox("球体（ミサイル頭）を表示", &showSphere);
		ImGui::Checkbox("通常リングを表示", &showNormalRing);
		ImGui::Checkbox("部分リングを表示", &showPartialRing);
		ImGui::Checkbox("シリンダーを表示", &showCylinder);
		ImGui::Checkbox("トレイルを表示", &showTrail);
		ImGui::Checkbox("モデルを表示", &showModel);
		ImGui::Checkbox("パーティクルを表示", &showParticles);
		ImGui::Checkbox("ボーンを表示", &showBones);
		ImGui::Checkbox("スプライトを表示", &showSprite);
		ImGui::Checkbox("Show Debug Colliders", &showDebugColliders);
		bool showEnemyPaths = Enemy::IsPathVisualizationEnabled();
		if (ImGui::Checkbox("敵の飛行ルートを表示", &showEnemyPaths)) {
			Enemy::SetPathVisualizationEnabled(showEnemyPaths);
		}

		ImGui::Separator();
		ImGui::Text("デバック　カメラ");
		if (ImGui::Button(isDebugCameraActive_ ? "Switch to Player Camera" : "Switch to Debug Camera")) {
			SetDebugCameraActive(!isDebugCameraActive_);
		}
		ImGui::SameLine();
		ImGui::Text("%s", isDebugCameraActive_ ? "Active: Debug Camera" : "Active: Player Camera");

		ImGui::Separator();
		ImGui::Text("GPUパーティクルの操作");
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
		ImGui::Text("アニメーション制御");
		const char *animationNames[] = { "AnimatedCube", "simpleSkin", "sneakWalk", "walk" };
		if (ImGui::Combo("アニメーション", &currentAnimationIndex, animationNames, IM_ARRAYSIZE(animationNames))) {
			std::string dir, file, loadFile;
			if (currentAnimationIndex == 0) { dir = "resources/AnimatedCube"; file = "AnimatedCube.gltf"; loadFile = "AnimatedCube/AnimatedCube.gltf"; } else if (currentAnimationIndex == 1) { dir = "resources/simpleSkin"; file = "simpleSkin.gltf"; loadFile = "simpleSkin/simpleSkin.gltf"; } else if (currentAnimationIndex == 2) { dir = "resources/human"; file = "sneakWalk.gltf"; loadFile = "human/sneakWalk.gltf"; } else if (currentAnimationIndex == 3) { dir = "resources/human"; file = "walk.gltf"; loadFile = "human/walk.gltf"; }

			animationData = LoadAnimationFile(dir, file);
			Node rootNode = Model::LoadNodeHierarchy(dir, file);
			skeleton = CreateSkeleton(rootNode);
			if (!skeleton.joints.empty()) {
				skeleton.joints[skeleton.root].transform.translate = { 0.0f, 0.0f, 0.0f };
			}
			animationTime = 0.0f;

			ModelManager::GetInstance()->LoadModel(loadFile);
			myModelObject->SetModel(loadFile);
			if (myModelObject->GetModel()) {
				myModelObject->skinCluster = myModelObject->GetModel()->CreateSkinCluster(skeleton);
			}
			showModel = true;
			enableSkinning = true;

			// モデルに応じた適切なスケールを自動設定する
			if (currentAnimationIndex == 0) {
				modelScale = 1.0f;
			} else {
				modelScale = 1.0f;  // simpleSkinなどは等倍で人間サイズ
			}
		}
		ImGui::Checkbox("スキニング (ガワを動かす)", &enableSkinning);
		ImGui::SliderFloat("モデルスケール", &modelScale, 0.001f, 1.0f);
		if (ImGui::Checkbox("アニメーション再生", &playAnimation)) {}
		ImGui::SliderFloat("再生時間", &animationTime, 0.0f, animationData.duration);

		ImGui::Separator();
		ImGui::Text("シリンダー設定");
		ImGui::DragFloat3("シリンダー座標", cylinderPos, 0.01f);
		ImGui::DragFloat3("シリンダースケール", cylinderScale, 0.01f);
		ImGui::DragFloat2("UVスクロール速度", cylinderUVScrollSpeed, 0.001f);
		ImGui::SliderFloat("アルファリファレンス", &cylinderAlphaReference, 0.0f, 1.0f);

		ImGui::Separator();
		missilePresetManager_->DrawMissileSettingsUI();

		bool cChanged = false;
		if (ImGui::SliderInt("Subdivision##Cyl", &cylinderSubdivision, 3, 128)) cChanged = true;
		if (ImGui::SliderInt("Vertical Subdivision", &cylinderVerticalSubdivision, 1, 32)) cChanged = true;
		if (ImGui::DragFloat("Top Radius X", &cylinderTopRadiusX, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Top Radius Z", &cylinderTopRadiusZ, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Bottom Radius X", &cylinderBottomRadiusX, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Bottom Radius Z", &cylinderBottomRadiusZ, 0.01f)) cChanged = true;
		if (ImGui::DragFloat("Height", &cylinderHeight, 0.01f)) cChanged = true;
		if (ImGui::ColorEdit4("Top Color", cylinderTopColor)) cChanged = true;
		if (ImGui::ColorEdit4("Bottom Color", cylinderBottomColor)) cChanged = true;
		if (ImGui::SliderFloat("Start Angle##Cyl", &cylinderStartAngle, 0.0f, 360.0f)) cChanged = true;
		if (ImGui::SliderFloat("End Angle##Cyl", &cylinderEndAngle, 0.0f, 360.0f)) cChanged = true;
		if (ImGui::Checkbox("Flip UV", &cylinderIsUvFlipped)) cChanged = true;

		if (myCylinder && cChanged) {
			Model *model = myCylinder->GetModel();
			if (model) {
				model->InitializeCylinder(model->GetModelCommon(),
					cylinderSubdivision, cylinderVerticalSubdivision,
					cylinderTopRadiusX, cylinderTopRadiusZ,
					cylinderBottomRadiusX, cylinderBottomRadiusZ,
					cylinderHeight,
					{ cylinderTopColor[0], cylinderTopColor[1], cylinderTopColor[2], cylinderTopColor[3] },
					{ cylinderBottomColor[0], cylinderBottomColor[1], cylinderBottomColor[2], cylinderBottomColor[3] },
					cylinderStartAngle, cylinderEndAngle,
					cylinderIsUvFlipped);
			}
		}

		ImGui::Separator();
		ImGui::Text("Partial Ring Settings");

		bool pRingChanged = false;
		if (ImGui::SliderInt("Subdivision", &prSubdivision, 3, 128)) pRingChanged = true;
		if (ImGui::SliderFloat("Outer Radius", &prOuterRadius, 0.1f, 5.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("Inner Radius", &prInnerRadius, 0.1f, 5.0f)) pRingChanged = true;
		if (ImGui::Checkbox("UV Horizontal", &prIsUvHorizontal)) pRingChanged = true;
		if (ImGui::ColorEdit4("Inner Color", prInnerColor)) pRingChanged = true;
		if (ImGui::ColorEdit4("Outer Color", prOuterColor)) pRingChanged = true;
		if (ImGui::SliderFloat("Start Angle", &prStartAngle, 0.0f, 360.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("End Angle", &prEndAngle, 0.0f, 360.0f)) pRingChanged = true;
		if (ImGui::SliderFloat("Fade Angle", &prFadeAngle, 0.0f, 180.0f)) pRingChanged = true;

		if (pRingChanged && myPartialRing) {
			Model *model = myPartialRing->GetModel();
			if (model) {
				model->InitializeRing(
					model->GetModelCommon(), prSubdivision, prOuterRadius, prInnerRadius,
					prIsUvHorizontal, { prInnerColor[0], prInnerColor[1], prInnerColor[2], prInnerColor[3] },
					{ prOuterColor[0], prOuterColor[1], prOuterColor[2], prOuterColor[3] },
					prStartAngle, prEndAngle, prFadeAngle
				);
			}
		}

		//スプライトの座標を配列に格納
		Vector2 currentPos = sprite->GetPosition();
		float pos[2] = { currentPos.x,currentPos.y };

		//スライダーでスプライトの座標を変更できるようにする
		ImGui::SliderFloat2("Sprite Position", pos, 0.0f, 1280.0f, "%.1f");

		//スライダーで変更された座標をスプライトに反映
		sprite->SetPosition({ pos[0],pos[1] });

		ImGui::Separator();
		ImGui::Text("イベントツール（増援設定）");
        
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

			ImGui::Separator();
			ImGui::Text("設定済みのイベント一覧");
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

		}

		if (currentEngineSettingsTarget_ == 1) {

		// =====================================================
		if (isDebugCameraActive_) {
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[FREE CAM ACTIVE]");
			ImGui::Text("WASD: 移動  /  矢印キー: 回転  /  Q,E: ロール");
			ImGui::Text("Game View上で右ドラッグ: 回転 / 中ドラッグ: パン / ホイール: 前後移動");

			if (ImGui::Button("自機追従カメラに戻る")) {
				SetDebugCameraActive(false);
			}
			ImGui::SameLine();
			if (ImGui::Button("自機後方にリセット")) {
				scene_->ResetDebugCameraToPlayer();
			}

			ImGui::Separator();
			ImGui::Text("操作方法:");
			ImGui::BulletText("右クリック + ドラッグ : 視点回転");
			ImGui::BulletText("スクロールホイール   : 前後移動");
			ImGui::BulletText("中クリック + ドラッグ : 上下左右パン");
			ImGui::BulletText("WASD                  : 前後左右移動");
			ImGui::BulletText("矢印キー              : 視点回転(キーボード)");
			ImGui::BulletText("Q / E                 : ロール");

			ImGui::Separator();
			// フリーカメラの速度調整（カメラから現在値を読む）
			float moveSpd = debugFlyCamera_->GetMoveSpeed();
			float rotSpd  = debugFlyCamera_->GetRotateSpeed();
			float sens    = debugFlyCamera_->GetMouseSensitivity();
			float scroll  = debugFlyCamera_->GetScrollSpeed();
			float pan     = debugFlyCamera_->GetPanSpeed();
			if (ImGui::DragFloat("移動速度 (WASD)##fly",      &moveSpd, 0.01f, 0.01f, 20.0f)) {
				debugFlyCamera_->SetMoveSpeed(moveSpd);
			}
			if (ImGui::DragFloat("回転感度 (マウス右)##fly",  &sens,    0.0001f, 0.0001f, 0.05f, "%.4f")) {
				debugFlyCamera_->SetMouseSensitivity(sens);
			}
			if (ImGui::DragFloat("スクロール速度##fly",       &scroll,  0.1f, 0.1f, 20.0f)) {
				debugFlyCamera_->SetScrollSpeed(scroll);
			}
			if (ImGui::DragFloat("パン速度 (中ボタン)##fly",  &pan,     0.001f, 0.001f, 1.0f)) {
				debugFlyCamera_->SetPanSpeed(pan);
			}
			if (ImGui::DragFloat("回転速度 (キーボード)##fly",&rotSpd,  0.001f, 0.001f, 0.5f)) {
				debugFlyCamera_->SetRotateSpeed(rotSpd);
			}

			ImGui::Separator();
			// フリーカメラの現在位置表示
			Vector3 flyPos = debugFlyCamera_->GetTranslate();
			float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
			if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) {
				debugFlyCamera_->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
			}

		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[PLAYER FOLLOW CAM]");
			ImGui::Text("ボタンを押すとフリーカメラに切り替わります");

			if (ImGui::Button("フリーカメラに切り替え")) {
				SetDebugCameraActive(true);
			}

			ImGui::Separator();
			// 自機追従カメラの現在位置表示（参考用）
			if (ImGui::Checkbox("Cinematic lock-on camera", &isCinematicLockOnCameraEnabled_)) {
				isCinematicLockOnCameraInitialized_ = false;
			}
			ImGui::Text("Cinematic: %s", (isCinematicLockOnCameraEnabled_ && lockedEnemy_) ? "ACTIVE" : "OFF");
			ImGui::Separator();

			Vector3 camPos = camera->GetTranslate();
			float camPosArr[3] = { camPos.x, camPos.y, camPos.z };
			if (ImGui::DragFloat3("カメラ位置 (参考)##follow", camPosArr, 0.1f)) {
				camera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
			}
		}

		}

		if (currentEngineSettingsTarget_ == 2) {
		ImGui::Text("Blender互換の敵配置");
		ImGui::TextWrapped("ここで追加・編集した敵は Blender と共通の scene.json に保存されます。座標はゲーム座標で入力し、保存時に Blender 座標へ自動変換します。");
		ImGui::Separator();

		const char* enemyTypes[] = { "VF1", "VF1-1", "VF3", "Jammer", "Boss" };
		ImGui::InputText("敵の名前", blenderEnemyName_, IM_ARRAYSIZE(blenderEnemyName_));
		ImGui::Combo("敵タイプ", &blenderEnemyTypeIndex_, enemyTypes, IM_ARRAYSIZE(enemyTypes));
		ImGui::DragFloat3("配置座標 (Game X,Y,Z)", blenderEnemyPosition_, 0.1f);
		ImGui::DragFloat3("回転 (度)", blenderEnemyRotationDegrees_, 1.0f);
		if (ImGui::Checkbox("Game View をクリックして敵を配置", &enemyMousePlacementEnabled_) && enemyMousePlacementEnabled_) {
			SetDebugCameraActive(true);
		}
		if (enemyMousePlacementEnabled_) {
			ImGui::DragFloat("クリック配置の高さ (Y)", &enemyMousePlacementHeight_, 0.1f, -100.0f, 300.0f);
			ImGui::TextColored(ImVec4(0.35f, 0.9f, 1.0f, 1.0f), "フリーカメラで見ながら Game View を左クリックすると敵を配置します。");
		}

		if (ImGui::Button("敵を配置して scene.json に保存")) {
			const float degreesToRadians = 3.141592654f / 180.0f;
			const Vector3 position = { blenderEnemyPosition_[0], blenderEnemyPosition_[1], blenderEnemyPosition_[2] };
			const Vector3 rotation = {
				blenderEnemyRotationDegrees_[0] * degreesToRadians,
				blenderEnemyRotationDegrees_[1] * degreesToRadians,
				blenderEnemyRotationDegrees_[2] * degreesToRadians,
			};
			simulationManager_->AddEnemySpawnToSceneJson(
				"resources/scene.json", blenderEnemyName_, enemyTypes[blenderEnemyTypeIndex_], position, rotation);
		}

		if (!simulationSaveMessage_.empty()) {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", simulationSaveMessage_.c_str());
		}

		ImGui::Separator();
		ImGui::Text("配置済みの敵 (総数: %d)", static_cast<int>(enemySpawns_.size()));
		int spawnOrderSource = -1;
		int spawnOrderTarget = -1;
		for (size_t spawnIndex = 0; spawnIndex < enemySpawns_.size(); ++spawnIndex) {
			EnemySpawnData &spawn = enemySpawns_[spawnIndex];
			ImGui::PushID(static_cast<int>(spawnIndex));
			if (ImGui::TreeNode(spawn.name.c_str())) {
				ImGui::Text("初期スポーン順: %d", static_cast<int>(spawnIndex + 1));
				if (spawnIndex > 0 && ImGui::SmallButton("上へ")) {
					spawnOrderSource = static_cast<int>(spawnIndex);
					spawnOrderTarget = static_cast<int>(spawnIndex - 1);
				}
				ImGui::SameLine();
				if (spawnIndex + 1 < enemySpawns_.size() && ImGui::SmallButton("下へ")) {
					spawnOrderSource = static_cast<int>(spawnIndex);
					spawnOrderTarget = static_cast<int>(spawnIndex + 1);
				}
				float position[3] = { spawn.position.x, spawn.position.y, spawn.position.z };
				float rotationDegrees[3] = {
					spawn.rotation.x * 180.0f / 3.141592654f,
					spawn.rotation.y * 180.0f / 3.141592654f,
					spawn.rotation.z * 180.0f / 3.141592654f,
				};
				bool changed = ImGui::DragFloat3("座標 (Game X,Y,Z)", position, 0.1f);
				changed |= ImGui::DragFloat3("回転 (度)", rotationDegrees, 1.0f);
				if (changed) {
					spawn.position = { position[0], position[1], position[2] };
					const float degreesToRadians = 3.141592654f / 180.0f;
					spawn.rotation = {
						rotationDegrees[0] * degreesToRadians,
						rotationDegrees[1] * degreesToRadians,
						rotationDegrees[2] * degreesToRadians,
					};
					for (const auto &enemy : enemies_) {
						if (enemy && enemy->GetSpawnPointIndex() == spawnIndex) {
							enemy->SetPosition(spawn.position);
							enemy->SetRotation(spawn.rotation);
							break;
						}
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
			if (spawnOrderTarget >= 0) {
				break;
			}
		}

		if (spawnOrderSource >= 0 && spawnOrderTarget >= 0) {
			simulationManager_->MoveEnemySpawnInSceneJson(
				"resources/scene.json", static_cast<size_t>(spawnOrderSource), static_cast<size_t>(spawnOrderTarget));
		}

		if (ImGui::Button("配置済みの敵の変更を scene.json に保存")) {
			simulationManager_->SaveCurrentSimulationLayoutToSceneJson("resources/scene.json");
		}

		ImGui::Separator();
		ImGui::Text("=== 障害物のリスト (総数: %d) ===", (int)obstacles_.size());
		
		if (ImGui::Button("障害物の設定をJSONに保存")) {
			simulationManager_->SaveCurrentSimulationLayoutToSceneJson("resources/scene.json");
		}
		if (!simulationSaveMessage_.empty()) {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", simulationSaveMessage_.c_str());
		}

		int obsIndex = 0;
		for (const auto& obstacle : obstacles_) {
			Vector3 pos = obstacle->GetPosition();
			Vector3 colOff = obstacle->GetCollisionOffset();
			Vector3 colScale = obstacle->GetCollisionScale();
			bool colEnabled = obstacle->IsCollisionEnabled();
			bool useMeshCol = obstacle->IsUseMeshCollider();

			ImGui::PushID(obsIndex);
			ImGui::Text("[%d] 位置: (%.2f, %.2f, %.2f)", obsIndex, pos.x, pos.y, pos.z);
			
			if (ImGui::Checkbox("Collision Enabled", &colEnabled)) {
				obstacle->SetCollisionEnabled(colEnabled);
			}
			if (ImGui::Checkbox("Use Mesh Collider", &useMeshCol)) {
				obstacle->SetUseMeshCollider(useMeshCol);
			}
			if (ImGui::DragFloat3("Collision Offset", &colOff.x, 0.1f)) {
				obstacle->SetCollisionOffset(colOff);
			}
			if (ImGui::DragFloat3("Collision Scale", &colScale.x, 0.05f)) {
				obstacle->SetCollisionScale(colScale);
			}
			ImGui::PopID();
			
			obsIndex++;
		}
		if (obstacles_.empty()) {
			ImGui::Text("現在、障害物は存在しません。");
		}
		}

		if (currentEngineSettingsTarget_ == 3) {
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

			ImGui::Separator();
			if (ImGui::Button("設定をJSONに保存")) {
				explosionManager_->SaveToJson("resources/explosionConfig.json");
			}
			ImGui::SameLine();
			if (ImGui::Button("設定をJSONから読込")) {
				explosionManager_->LoadFromJson("resources/explosionConfig.json");
			}
		}
		}

#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
		const StageValidation::Report &stageValidationReport = StageValidation::GetLastReport();
		if (stageValidationReport.HasMessages() || stageValidationReport.HasCheckItems()) {
			if (gShowStageValidationWindow) {
				DrawStageValidationWindow(stageValidationReport);
			}

			if (gShowStageValidationLabels) {
				Camera *validationCamera = isDebugCameraActive_ ? static_cast<Camera *>(debugFlyCamera_.get()) : camera.get();
				if (validationCamera) {
					DrawStageValidationOverlay(stageValidationReport, validationCamera->GetViewProjectionMatrix());
				}
			}
		}
#endif

		if (currentEngineSettingsTarget_ == 4) {

#if defined(ENABLE_IMGUI) && defined(CG2_ENABLE_STAGE_VALIDATION)
		ImGui::Text("レベル検査表示");
		ImGui::Checkbox("検査一覧を表示", &gShowStageValidationWindow);
		ImGui::Checkbox("警告ラベルを表示", &gShowStageValidationLabels);
		ImGui::Separator();
#endif

		// もしボタンが押されたら { } の中が実行される
		if (ImGui::Button("Open Blender")) {
			OpenStageBlendFile();
		}
		if (!gBlenderLaunchMessage.empty()) {
			ImGui::TextWrapped("%s", gBlenderLaunchMessage.c_str());
		}

		ImGui::Separator();
		ImGui::Text("敵機ルート確認");
		if (ImGui::Button("リセット")) {
			ResetEditorPreview();
		}
		ImGui::SameLine();
		if (ImGui::Button("再生")) {
			isEditorPreviewPlaying_ = true;
			OutputDebugStringA("[EditorPreview] Play.\n");
		}
		ImGui::SameLine();
		if (ImGui::Button("ストップ")) {
			isEditorPreviewPlaying_ = false;
			OutputDebugStringA("[EditorPreview] Stop.\n");
		}

		ImGui::TextColored(
			isEditorPreviewPlaying_ ? ImVec4(0.0f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
			"状態: %s",
			isEditorPreviewPlaying_ ? "再生中" : "停止中");

		ImGui::TextWrapped("リセットでscene.jsonを読み直して初期状態に戻し、停止状態にします。再生で敵機ルートなどのゲーム更新が進み、ストップでその場に止まります。");

		}

		if (currentEngineSettingsTarget_ == 5) {
			Input *input = Input::GetInstance();
			ImGui::Text("プレイヤー操作設定");
			ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "射撃: 左クリック = 通常射撃 / 右クリック = ホーミング射撃");
			ImGui::TextDisabled("変更後にキー、マウスボタン、またはホイールを操作して割り当てます。");
			ImGui::TextColored(
				input->IsControllerConnected() ? ImVec4(0.25f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.70f, 0.25f, 1.0f),
				"コントローラー: %s (XInput Player 1)", input->IsControllerConnected() ? "接続済み" : "未接続");

			bool isCursorClipped = input->IsMouseCursorClipEnabled();
			if (ImGui::Checkbox("マウスカーソルを画面内に固定する (画面外へのはみ出し防止)", &isCursorClipped)) {
				input->SetMouseCursorClipEnabled(isCursorClipped);
			}

			if (ImGui::Button("初期設定に戻す")) {
				input->ResetPlayerActionBindings();
				input->SavePlayerActionBindings();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("変更は次回起動時も維持されます。");
			ImGui::Separator();

			static int waitingForKeyboardAction = -1;
			static bool ignoreMouseCaptureUntilReleased = false;
			for (size_t actionIndex = 0; actionIndex < Input::GetPlayerActionCount(); ++actionIndex) {
				const PlayerAction action = static_cast<PlayerAction>(actionIndex);
				const PlayerActionBinding &binding = input->GetActionBinding(action);
				ImGui::PushID(static_cast<int>(actionIndex));
				ImGui::Text("%s", Input::GetPlayerActionName(action));
				ImGui::SameLine(155.0f);
				if (waitingForKeyboardAction == static_cast<int>(actionIndex)) {
					ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.20f, 1.0f), "キーまたはマウスを操作してください");
					for (int keyNumber = 1; keyNumber < 256; ++keyNumber) {
						if (!input->TriggerKey(static_cast<BYTE>(keyNumber))) continue;
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
							if (!input->TriggerMouseButton(button)) continue;
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
					if (ImGui::SmallButton("変更")) {
						waitingForKeyboardAction = static_cast<int>(actionIndex);
						ignoreMouseCaptureUntilReleased = true;
					}
				}

				ImGui::SameLine(320.0f);
				ImGui::SetNextItemWidth(155.0f);
				if (ImGui::BeginCombo("##controller", Input::GetControllerInputName(binding.controllerInput))) {
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
				ImGui::SameLine(320.0f);
				ImGui::SetNextItemWidth(155.0f);
				if (ImGui::BeginCombo("##mouse", Input::GetMouseInputName(binding.mouseInput))) {
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
		}

		if (currentEngineSettingsTarget_ == 6) {
			static std::string packageMessage;
			const bool projectFound = !FindProjectRoot().empty();
			const bool runningRelease = IsCurrentExecutableReleaseBuild();
			ImGui::Text("実行ファイル生成");
			ImGui::TextWrapped("Release版のCG2.exe、DLL、resourcesをまとめて、配布用ZIPを作成します。");
			ImGui::Separator();
			if (!projectFound) {
				ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "プロジェクトフォルダーから起動した場合のみ利用できます。");
			} else if (runningRelease) {
				ImGui::TextDisabled("現在Release版を実行中です。ビルドは行わず、現在のRelease版をZIPにします。");
				if (ImGui::Button("現在のRelease版をZIP作成")) {
					packageMessage = CreateExecutablePackage(true)
						? "PowerShellでZIP作成を開始しました。完了後に generated/packages を確認してください。"
						: "ZIP作成を開始できませんでした。";
				}
			} else if (ImGui::Button("ReleaseビルドしてZIP作成")) {
				packageMessage = CreateExecutablePackage(false)
					? "PowerShellでReleaseビルドとZIP作成を開始しました。完了後に generated/packages を確認してください。"
					: "実行ファイル生成を開始できませんでした。";
			}
			ImGui::TextDisabled("出力先: generated/packages/CG2_Playable-Release.zip");
			if (!packageMessage.empty()) {
				ImGui::Spacing();
				ImGui::TextWrapped("%s", packageMessage.c_str());
			}
		}

		if (currentEngineSettingsTarget_ == 7) {
			ImGui::Text("スラスターの見た目・取付位置");
			ImGui::TextWrapped("炎の幅・長さ・明るさと取付位置を調整できます。変更はゲーム画面に即反映します。");
			ImGui::TextWrapped("ゲーム／シミュレーションの編集停止中も出力65%で噴射を表示します。通常・加速の見え方を切り替えて比較できます。");
			ImGui::Separator();
			if (player_ && player_->GetBoosterEffect()) {
				BoosterEffect* booster = player_->GetBoosterEffect();
				bool previewAccelerating = booster->GetEditorPreviewAcceleration();
				if (ImGui::Checkbox("加速状態をプレビュー", &previewAccelerating)) {
					booster->SetEditorPreviewAcceleration(previewAccelerating);
				}
				ImGui::Separator();
				ImGui::Text("エフェクトの見た目");
				BoosterEffect::EffectSettings& effect = booster->GetEffectSettings();
				ImGui::SliderFloat("炎の幅", &effect.widthScale, 0.25f, 6.0f, "%.2f 倍", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SliderFloat("炎の長さ", &effect.lengthScale, 0.25f, 4.0f, "%.2f 倍", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SliderFloat("明るさ", &effect.brightness, 0.1f, 3.0f, "%.2f 倍", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SliderFloat("周囲の光", &effect.glowScale, 0.0f, 3.0f, "%.2f 倍", ImGuiSliderFlags_AlwaysClamp);
				if (ImGui::TreeNode("加速・白い芯の調整")) {
					ImGui::SliderFloat("加速時の幅倍率", &effect.boostWidthMultiplier, 1.0f, 2.0f, "%.2f 倍", ImGuiSliderFlags_AlwaysClamp);
					ImGui::SliderFloat("加速時の長さ倍率", &effect.boostLengthMultiplier, 1.0f, 2.0f, "%.2f 倍", ImGuiSliderFlags_AlwaysClamp);
					ImGui::SliderFloat("白い芯の長さ比率", &effect.coreLengthRatio, 0.2f, 0.8f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
					ImGui::TreePop();
				}
				if (ImGui::Button("エフェクトの見た目をリセット", ImVec2(-1.0f, 0.0f))) {
					booster->ResetEffectSettings();
				}
				ImGui::Separator();
				ImGui::Text("取付位置（機体ローカル・メートル）");
				ImGui::Checkbox("位置合わせ用マーカー", &showThrusterPlacementMarkers_);
				booster->SetPlacementMarkersVisible(showThrusterPlacementMarkers_);
				Vector3& offset = booster->GetPlacementAdjustment();
				if (ImGui::DragFloat3("位置補正 X・Y・Z", &offset.x, 0.01f, -3.0f, 3.0f, "%.3f m")) {
					booster->RefreshPlacement(player_->GetPosition(), player_->GetQuaternion(),
						static_cast<int>(player_->GetCurrentMode()));
				}
				ImGui::TextDisabled("+X: 右 / +Y: 上 / +Z: 前方。Yをマイナスにすると下がります。");
				if (ImGui::Button("位置補正をリセット", ImVec2(-1.0f, 0.0f))) {
					booster->ResetPlacementAdjustment();
					booster->RefreshPlacement(player_->GetPosition(), player_->GetQuaternion(),
						static_cast<int>(player_->GetCurrentMode()));
				}
				if (ImGui::Button("スラスター設定を保存", ImVec2(-1.0f, 0.0f))) {
					thrusterSettingsMessage_ = booster->SaveSettings("resources/vf-15c/thruster_position.json")
						? "取付位置とエフェクト設定を保存しました。次回起動時にも反映されます。"
						: "保存できませんでした。resources/vf-15c に書き込み可能か確認してください。";
				}
				if (ImGui::Button("保存設定を読み込む", ImVec2(-1.0f, 0.0f))) {
					if (booster->LoadSettings("resources/vf-15c/thruster_position.json")) {
						booster->RefreshPlacement(player_->GetPosition(), player_->GetQuaternion(),
							static_cast<int>(player_->GetCurrentMode()));
						thrusterSettingsMessage_ = "取付位置とエフェクト設定を読み込みました。";
					} else {
						thrusterSettingsMessage_ = "保存データを読み込めません。先に設定を保存するか、ファイル内容を確認してください。";
					}
				}
				if (!thrusterSettingsMessage_.empty()) {
					ImGui::TextWrapped("%s", thrusterSettingsMessage_.c_str());
				}
				ImGui::TextDisabled("保存先: resources/vf-15c/thruster_position.json");
			} else {
				ImGui::TextDisabled("プレイヤーが初期化されていません。");
			}
		}
		if (currentEngineSettingsTarget_ != 7 && player_ && player_->GetBoosterEffect()) {
			player_->GetBoosterEffect()->SetPlacementMarkersVisible(false);
		}

		ImGui::End();
	}
#endif

#undef mode_
#undef camera
#undef sprite
#undef groundModel
#undef myShere
#undef skybox
#undef aimCursorSprite_
#undef lockOnReticleSprite_
#undef boundaryAlertObject_
#undef ceilingBoundaryAlertObject_
#undef particleManager
#undef particleEmitter
#undef soundData1
#undef soundData2
#undef boundaryAlertPlane_
#undef myBox
#undef myRing
#undef myPartialRing
#undef myCylinder
#undef myModelObject
#undef showNormalRing
#undef showPartialRing
#undef showCylinder
#undef prSubdivision
#undef prOuterRadius
#undef prInnerRadius
#undef prIsUvHorizontal
#undef prInnerColor
#undef prOuterColor
#undef prStartAngle
#undef prEndAngle
#undef prFadeAngle
#undef cylinderPos
#undef cylinderScale
#undef cylinderUVOffset
#undef cylinderUVScrollSpeed
#undef cylinderAlphaReference
#undef cylinderSubdivision
#undef cylinderVerticalSubdivision
#undef cylinderTopRadiusX
#undef cylinderTopRadiusZ
#undef cylinderBottomRadiusX
#undef cylinderBottomRadiusZ
#undef cylinderHeight
#undef cylinderTopColor
#undef cylinderBottomColor
#undef cylinderStartAngle
#undef cylinderEndAngle
#undef cylinderIsUvFlipped
#undef animationData
#undef animationTime
#undef playAnimation
#undef skeleton
#undef showBones
#undef showPlane
#undef showSphere
#undef showBox
#undef showTrail
#undef showSkybox
#undef showSprite
#undef skeletonLinesModel
#undef skeletonLinesObject
#undef debugColliderLinesObject
#undef showDebugColliders
#undef debugFlyCamera_
#undef isDebugCameraActive_
#undef isEditorPreviewPlaying_
#undef isCinematicLockOnCameraEnabled_
#undef isCinematicLockOnCameraInitialized_
#undef cinematicLockOnCameraPosition_
#undef cinematicLockOnCameraRotation_
#undef cinematicLockOnCameraFocus_
#undef cinematicLockOnCameraBackDirection_
#undef cinematicLockOnCameraSideSign_
#undef cinematicLockOnCameraSeparation_
#undef showParticles
#undef showModel
#undef enableSkinning
#undef modelScale
#undef currentAnimationIndex
#undef missileTrail
#undef trailObject
#undef missileNormalSpeed
#undef missileNormalScale
#undef missileNormalCollisionRadius
#undef missileNormalLifeTime
#undef missileSpeed
#undef missileAmpX
#undef missileAmpZ
#undef missileAmpY
#undef missileFreqY
#undef missileBaseY
#undef missileHomingStrength
#undef missileHomingScale
#undef missileHomingCollisionRadius
#undef missileTrailWidth
#undef missileLifeTime
#undef missileMuzzleOffset
#undef player_
#undef missileManager_
#undef enemies_
#undef enemyBulletManager_
#undef enemySpawns_
#undef enemyRespawnTimers_
#undef obstacles_
#undef newEnemyPos
#undef explosionManager_
#undef isGameOver_
#undef gameOverTimer_
#undef simulationManager_
#undef missilePresetManager_
#undef lockOnManager_
#undef cameraManager_
#undef levelManager_
#undef uiManager_
#undef ammoManager_
#undef lastJsonWriteTime_
#undef aimAssistEnemy_
#undef isMultiLockCharging_
#undef multiLockChargeFrames_
#undef IsSimulationMode
#undef SetDebugCameraActive
#undef ReloadSceneJson
#undef ResetEditorPreview
#undef MakeMissileTuning
#undef SpawnEnemyFromSpawnPoint
#undef IsEnemySpawnPointActive
#undef ScheduleEnemySpawn
#undef TriggerEnemyReinforcements
#undef UpdateEnemyRespawns
#undef HasPendingEnemySpawns
#undef UpdateCinematicLockOnCamera


#undef enemyEventManager_
#undef lockedEnemy_
#undef aimAssistEnemy_
#undef isMultiLockCharging_
#undef multiLockTargets_
}

void GamePlayUIManager::DrawGameplayActionControls() {
#ifdef ENABLE_IMGUI
	ImGui::Separator();
	ImGui::Text("保存済みシミュレーション設定");
	ImGui::TextWrapped("シミュレーション画面で保存した内容を、現在のゲーム側の設定値として読み込みます。");
	if (ImGui::Button("保存一覧を更新")) {
		scene_->simulationManager_->RefreshSimulationActionNames();
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
		scene_->simulationManager_->ApplySimulationAction(kSimulationActionsFilePath, simulationActionNames_[selectedSimulationActionIndex_]);
	}

	if (!simulationActionMessage_.empty()) {
		ImGui::TextWrapped("%s", simulationActionMessage_.c_str());
	}
#endif
}

void GamePlayUIManager::DrawAmmoSettingsUI() {
#ifdef ENABLE_IMGUI
	if (!scene_ || !scene_->ammoManager_) {
		return;
	}

	AmmoManager* ammoManager = scene_->ammoManager_.get();
	ImGui::Separator();
	ImGui::Text("弾薬・所持弾数設定");
	ImGui::TextDisabled("プレイヤーの弾数（使い切ると自動補充されるマガジン容量）を設定します。");

	int normalCapacity = ammoManager->GetNormalMagazineCapacity();
	int homingCapacity = ammoManager->GetHomingMagazineCapacity();
	bool changed = false;
	changed |= ImGui::DragInt("通常弾の所持数(最大数)", &normalCapacity, 1, 1, 999);
	changed |= ImGui::DragInt("ミサイルの所持数(最大数)", &homingCapacity, 1, 1, 999);
	if (changed) {
		ammoManager->SetMagazineCapacities(normalCapacity, homingCapacity);
		ammoSettingsMessage_ = "弾薬数を更新しました。";
	}

	ImGui::Text("現在の残弾: 通常弾 %d / %d   ミサイル %d / %d",
		ammoManager->GetNormalMagazine(), ammoManager->GetNormalMagazineCapacity(),
		ammoManager->GetHomingMagazine(), ammoManager->GetHomingMagazineCapacity());

	if (ImGui::Button("弾薬を全回復(リロード)")) {
		ammoManager->ReloadAll();
		ammoSettingsMessage_ = "弾薬を全回復しました。";
	}
	ImGui::SameLine();
	if (ImGui::Button("弾薬設定を保存")) {
		ammoSettingsMessage_ = ammoManager->SaveSettings("resources/ammo_settings.json")
			? "弾薬設定を resources/ammo_settings.json に保存しました。"
			: "弾薬設定を保存できませんでした。";
	}
	ImGui::SameLine();
	if (ImGui::Button("保存設定を読込")) {
		ammoSettingsMessage_ = ammoManager->LoadSettings("resources/ammo_settings.json")
			? "保存した弾薬設定を読み込みました。"
			: "保存した弾薬設定を読み込めませんでした。";
	}
	if (!ammoSettingsMessage_.empty()) {
		ImGui::TextWrapped("%s", ammoSettingsMessage_.c_str());
	}
#endif
}
