#include "GamePlayUIManager.h"
#include "GamePlayScene.h"
#include "GamePlaySceneHelpers.h"
#include "Game/base/GameSettings.h"
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
#include <cstdio>
#include <filesystem>
#include <shellapi.h>
#include <cmath>
#include <vector>

GamePlayUIManager::GamePlayUIManager(GamePlayScene* scene) : scene_(scene) {
}

void GamePlayUIManager::Initialize() {
}

void GamePlayUIManager::DrawEditorToolbar() {
	if (!scene_) return;
	simulationControlPanel_.DrawEditorToolbar({
		&scene_->enemies_,
		&scene_->isEditorPreviewPlaying_,
		[this]() { scene_->ResetEditorPreview(); },
	});
}

void GamePlayUIManager::UpdateUI() {
    if (!scene_) return;
	effectTuningPanel_.UpdatePreviewTimer(ImGui::GetIO().DeltaTime);
#define enemyEventManager_ scene_->enemyEventManager_

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
#define showDebugColliders scene_->showDebugColliders
#define debugFlyCamera_ scene_->debugFlyCamera_
#define isDebugCameraActive_ scene_->isDebugCameraActive_
#define isEditorPreviewPlaying_ scene_->isEditorPreviewPlaying_
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
#define missileNormalHomingStrength scene_->missileNormalHomingStrength
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
#define obstacles_ scene_->obstacles_
#define newEnemyPos scene_->newEnemyPos
#define explosionManager_ scene_->explosionManager_
#define simulationManager_ scene_->simulationManager_
#define missilePresetManager_ scene_->missilePresetManager_
#define lockOnManager_ scene_->lockOnManager_
#define cameraManager_ scene_->cameraManager_
#define uiManager_ scene_->uiManager_
#define ammoManager_ scene_->ammoManager_
#define SetDebugCameraActive scene_->SetDebugCameraActive
#define ResetEditorPreview scene_->ResetEditorPreview
#define MakeMissileTuning scene_->MakeMissileTuning


#ifdef ENABLE_IMGUI
	if (ImGuiManager::IsVisible()) {
		DrawEditorToolbar();

		ImGui::Begin("ゲーム設定");
		ImGui::Text("ゲーム設定");
		ImGui::TextWrapped("ゲーム内の設定をここで確認・変更できます。");
		ImGui::Separator();
		ImGui::Text("音声設定（変更は自動保存されます）");
		GameSettings& gameSettings = GameSettings::GetInstance();
		bool gameplayBgmEnabled = gameSettings.IsGameplayBgmEnabled();
		if (ImGui::Checkbox("ゲームプレイBGM", &gameplayBgmEnabled)) {
			gameSettings.SetGameplayBgmEnabled(gameplayBgmEnabled);
		}
		bool nonGameplayBgmEnabled = gameSettings.IsNonGameplayBgmEnabled();
		if (ImGui::Checkbox("ゲームプレイ外のBGM", &nonGameplayBgmEnabled)) {
			gameSettings.SetNonGameplayBgmEnabled(nonGameplayBgmEnabled);
		}
		bool soundEffectsEnabled = gameSettings.IsSoundEffectsEnabled();
		if (ImGui::Checkbox("効果音（SE）", &soundEffectsEnabled)) {
			gameSettings.SetSoundEffectsEnabled(soundEffectsEnabled);
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
				missilePresetManager_->DrawMissileSettingsUI();
			}
			else if (currentSimulationTarget_ == 2) {
				ImGui::Text("=== 敵の出現とルート ===");
				ImGui::Text("Lock-on: %s", scene_->targetSelection_.HasLockedEnemy() ? "LOCKED" : "NONE");
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
					bool cinematicLockOnEnabled = scene_->IsCinematicLockOnCameraEnabled();
					if (ImGui::Checkbox("Cinematic lock-on camera", &cinematicLockOnEnabled)) {
						scene_->SetCinematicLockOnCameraEnabled(cinematicLockOnEnabled);
					}
					ImGui::Text("Cinematic: %s", (scene_->IsCinematicLockOnCameraEnabled() && scene_->targetSelection_.HasLockedEnemy()) ? "ACTIVE" : "OFF");
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
			"敵イベント・ボス戦",
		"境界シールド",
		"ゲームバランス",
		"チュートリアル",
		};

		simulationControlPanel_.DrawCategorySidebar(
			engineSettingsCategories, IM_ARRAYSIZE(engineSettingsCategories), currentEngineSettingsTarget_);

		ImGui::SetNextWindowSize(ImVec2(500.0f, 620.0f), ImGuiCond_Once);
		ImGui::Begin("設定内容");
		ImGui::Text("%s の設定", engineSettingsCategories[currentEngineSettingsTarget_]);
		ImGui::Separator();

		if (currentEngineSettingsTarget_ == 0) {
		simulationControlPanel_.DrawDebugDisplayControls({
			&showSkybox,
			&showPlane,
			&showSphere,
			&showNormalRing,
			&showPartialRing,
			&showCylinder,
			&showTrail,
			&showModel,
			&showParticles,
			&showBones,
			&showSprite,
			&showDebugColliders,
			isDebugCameraActive_,
			[this](bool active) { SetDebugCameraActive(active); },
		});

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

		systemSettingsPanel_.DrawEnemyEventToolSection(enemyEventManager_, enemySpawns_);

		}

		if (currentEngineSettingsTarget_ == 8) {
			bossEditorPanel_.Draw({
				&enemySpawns_,
				&enemyEventManager_,
				simulationManager_.get(),
				&enemies_,
				&selectedBossEditorPart_,
				bossBattlePosition_,
				&bossTuning_,
			});
		}

		if (currentEngineSettingsTarget_ == 1) {
			systemSettingsPanel_.DrawCameraDebugTab({
				isDebugCameraActive_,
				debugFlyCamera_.get(),
				camera.get(),
				scene_->IsCinematicLockOnCameraEnabled(),
				scene_->targetSelection_.HasLockedEnemy(),
				[this](bool active) { SetDebugCameraActive(active); },
				[this]() { scene_->ResetDebugCameraToPlayer(); },
				[this](bool enabled) { scene_->SetCinematicLockOnCameraEnabled(enabled); },
			});
		}

		if (currentEngineSettingsTarget_ == 2) {
			stageEditorPanel_.DrawEnemyAndObstaclePanel({
				blenderEnemyName_,
				IM_ARRAYSIZE(blenderEnemyName_),
				&blenderEnemyTypeIndex_,
				blenderEnemyPosition_,
				blenderEnemyRotationDegrees_,
				&enemyMousePlacementEnabled_,
				&enemyMousePlacementHeight_,
				&simulationSaveMessage_,
				&enemySpawns_,
				&enemies_,
				&obstacles_,
				simulationManager_.get(),
				[this](bool active) { SetDebugCameraActive(active); },
			});
		}

		if (currentEngineSettingsTarget_ == 3) {
			effectTuningPanel_.DrawDestructionEffectTab(explosionManager_.get(), camera.get(), player_.get());
		}

		{
			Camera* validationCamera = isDebugCameraActive_ ? static_cast<Camera*>(debugFlyCamera_.get()) : camera.get();
			stageEditorPanel_.DrawStageValidationOverlays(validationCamera);
		}

		if (currentEngineSettingsTarget_ == 4) {
			stageEditorPanel_.DrawBlenderAndPreviewPanel(
				isEditorPreviewPlaying_,
				[this]() { ResetEditorPreview(); });
		}

		if (currentEngineSettingsTarget_ == 5) {
			systemSettingsPanel_.DrawPlayerInputBindingsTab();
		}

		if (currentEngineSettingsTarget_ == 6) {
			stageEditorPanel_.DrawPackageGeneratorPanel();
		}

		if (currentEngineSettingsTarget_ == 9) {
			effectTuningPanel_.DrawBoundaryShieldTab(
				scene_->GetBoundaryShieldSettings(),
				scene_->GetPlayer());
		}

		if (currentEngineSettingsTarget_ == 10) {
			playerTuningPanel_.DrawGameplayBalanceTab({
				&scene_->GetGameplayBalanceSettings(),
				scene_->GetPlayer(),
				scene_->CollectGameplayBalanceMissileSettings(),
				[this](const GameplayBalanceMissileSettings& settings) { scene_->ApplyGameplayBalanceMissileSettings(settings); },
				[this]() { scene_->ApplyGameplayBalanceSettings(); },
				[this]() { return scene_->SaveGameplayBalanceSettings(); },
				[this]() { return scene_->LoadGameplayBalanceSettings(); },
				[this]() { DrawAmmoSettingsUI(); },
			});
		}

		if (currentEngineSettingsTarget_ == 11) {
			systemSettingsPanel_.DrawTutorialRequirementsTab(scene_->GetTutorialRequirements());
		}

		if (currentEngineSettingsTarget_ == 7) {
			playerTuningPanel_.DrawThrusterTuningTab(player_.get());
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
#undef showDebugColliders
#undef debugFlyCamera_
#undef isDebugCameraActive_
#undef isEditorPreviewPlaying_
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
#undef missileNormalHomingStrength
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
#undef obstacles_
#undef newEnemyPos
#undef explosionManager_
#undef simulationManager_
#undef missilePresetManager_
#undef lockOnManager_
#undef cameraManager_
#undef uiManager_
#undef ammoManager_
#undef SetDebugCameraActive
#undef ResetEditorPreview
#undef MakeMissileTuning


#undef enemyEventManager_
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
	if (!scene_) {
		return;
	}
	systemSettingsPanel_.DrawAmmoSettingsSection(scene_->ammoManager_.get());
}
