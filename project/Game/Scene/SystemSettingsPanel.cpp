#include "SystemSettingsPanel.h"

#include "AmmoManager.h"
#include "Game/enemy/EnemyEventManager.h"
#include "TutorialRequirementsManager.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Input/Input.h"
#include "engine/Utility/StageLoader.h"

#include <externals/imgui/imgui.h>
#include <string>
#include <vector>

void SystemSettingsPanel::DrawCameraDebugTab(const CameraDebugPanelContext& context) const {
#if defined(ENABLE_IMGUI)
	if (context.isDebugCameraActive) {
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "[FREE CAM ACTIVE]");
		ImGui::Text("WASD: 移動  /  矢印キー: 回転  /  Q,E: ロール");
		ImGui::Text("Game View上で右ドラッグ: 回転 / 中ドラッグ: パン / ホイール: 前後移動");

		if (ImGui::Button("自機追従カメラに戻る") && context.setDebugCameraActive) {
			context.setDebugCameraActive(false);
		}
		ImGui::SameLine();
		if (ImGui::Button("自機後方にリセット") && context.resetDebugCameraToPlayer) {
			context.resetDebugCameraToPlayer();
		}

		ImGui::Separator();
		ImGui::Text("操作方法:");
		ImGui::BulletText("右クリック + ドラッグ : 視点回転");
		ImGui::BulletText("スクロールホイール   : 前後移動");
		ImGui::BulletText("中クリック + ドラッグ : 上下左右パン");
		ImGui::BulletText("WASD                  : 前後左右移動");
		ImGui::BulletText("矢印キー              : 視点回転(キーボード)");
		ImGui::BulletText("Q / E                 : ロール");

		if (context.debugFlyCamera) {
			ImGui::Separator();
			// フリーカメラの速度調整（カメラから現在値を読む）
			float moveSpd = context.debugFlyCamera->GetMoveSpeed();
			float rotSpd = context.debugFlyCamera->GetRotateSpeed();
			float sens = context.debugFlyCamera->GetMouseSensitivity();
			float scroll = context.debugFlyCamera->GetScrollSpeed();
			float pan = context.debugFlyCamera->GetPanSpeed();
			if (ImGui::DragFloat("移動速度 (WASD)##fly", &moveSpd, 0.01f, 0.01f, 20.0f)) {
				context.debugFlyCamera->SetMoveSpeed(moveSpd);
			}
			if (ImGui::DragFloat("回転感度 (マウス右)##fly", &sens, 0.0001f, 0.0001f, 0.05f, "%.4f")) {
				context.debugFlyCamera->SetMouseSensitivity(sens);
			}
			if (ImGui::DragFloat("スクロール速度##fly", &scroll, 0.1f, 0.1f, 20.0f)) {
				context.debugFlyCamera->SetScrollSpeed(scroll);
			}
			if (ImGui::DragFloat("パン速度 (中ボタン)##fly", &pan, 0.001f, 0.001f, 1.0f)) {
				context.debugFlyCamera->SetPanSpeed(pan);
			}
			if (ImGui::DragFloat("回転速度 (キーボード)##fly", &rotSpd, 0.001f, 0.001f, 0.5f)) {
				context.debugFlyCamera->SetRotateSpeed(rotSpd);
			}

			ImGui::Separator();
			// フリーカメラの現在位置表示
			Vector3 flyPos = context.debugFlyCamera->GetTranslate();
			float flyPosArr[3] = { flyPos.x, flyPos.y, flyPos.z };
			if (ImGui::DragFloat3("カメラ位置##fly", flyPosArr, 0.1f)) {
				context.debugFlyCamera->SetTranslate({ flyPosArr[0], flyPosArr[1], flyPosArr[2] });
			}
		}
	} else {
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[PLAYER FOLLOW CAM]");
		ImGui::Text("ボタンを押すとフリーカメラに切り替わります");

		if (ImGui::Button("フリーカメラに切り替え") && context.setDebugCameraActive) {
			context.setDebugCameraActive(true);
		}

		ImGui::Separator();
		// 自機追従カメラの現在位置表示（参考用）
		bool cinematicLockOnEnabled = context.isCinematicLockOnEnabled;
		if (ImGui::Checkbox("Cinematic lock-on camera", &cinematicLockOnEnabled) && context.setCinematicLockOnEnabled) {
			context.setCinematicLockOnEnabled(cinematicLockOnEnabled);
		}
		ImGui::Text("Cinematic: %s", (cinematicLockOnEnabled && context.hasLockedEnemy) ? "ACTIVE" : "OFF");
		ImGui::Separator();

		if (context.followCamera) {
			Vector3 camPos = context.followCamera->GetTranslate();
			float camPosArr[3] = { camPos.x, camPos.y, camPos.z };
			if (ImGui::DragFloat3("カメラ位置 (参考)##follow", camPosArr, 0.1f)) {
				context.followCamera->SetTranslate({ camPosArr[0], camPosArr[1], camPosArr[2] });
			}
		}
	}
#else
	(void)context;
#endif
}

void SystemSettingsPanel::DrawPlayerInputBindingsTab() const {
#if defined(ENABLE_IMGUI)
	Input* input = Input::GetInstance();
	ImGui::Text("プレイヤー操作設定");
	ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "射撃: 左クリック = 通常射撃 / 右クリック = ホーミング射撃");
	ImGui::TextDisabled("変更後にキー、マウスボタン、またはホイールを操作して割り当てます。");
	ImGui::TextColored(
		input->IsControllerConnected() ? ImVec4(0.25f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.70f, 0.25f, 1.0f),
		"コントローラー: %s (XInput Player 1)", input->IsControllerConnected() ? "接続済み" : "未接続");

	ImGui::TextUnformatted("通常プレイ中はマウスカーソルを自動で画面内に固定します。");
	ImGui::TextUnformatted("編集UI・ポーズ中は固定を解除します。");

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
		const PlayerActionBinding& binding = input->GetActionBinding(action);
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
#endif
}

void SystemSettingsPanel::DrawTutorialRequirementsTab(TutorialRequirementsManager& requirementsManager) {
#if defined(ENABLE_IMGUI)
	auto requirements = requirementsManager.Get();
	bool changed = false;
	auto editRequirement = [&changed](const char* label, int* value, const char* format) {
		changed |= ImGui::SliderInt(label, value, 1, 20, format, ImGuiSliderFlags_AlwaysClamp);
	};
	ImGui::TextWrapped("チュートリアルの各操作に必要な回数です。進捗表示は達成率（%%）のまま、操作を行った回数で加算されます。");
	ImGui::TextDisabled("変更は進行中のチュートリアルにも即時反映されます。");
	ImGui::Separator();
	editRequirement("前進回数", &requirements.forward, "%d 回");
	editRequirement("高度操作回数", &requirements.altitude, "%d 回");
	editRequirement("旋回回数", &requirements.turn, "%d 回");
	editRequirement("ガウォーク上下移動回数", &requirements.gerwalkHover, "%d 回");
	editRequirement("ガウォーク横移動回数", &requirements.gerwalkStrafe, "%d 回");
	editRequirement("ロックオン回数", &requirements.lockOn, "%d 回");
	editRequirement("主砲発射数", &requirements.normalShots, "%d 発");
	editRequirement("ファイター変形回数", &requirements.fighterTransform, "%d 回");
	editRequirement("ホーミング発射数", &requirements.homingLaunches, "%d 発");
	if (changed) {
		requirementsManager.Set(requirements);
	}

	ImGui::Separator();
	if (ImGui::Button("チュートリアル設定を保存", ImVec2(-1.0f, 0.0f))) {
		requirementsManager.Set(requirements);
		tutorialRequirementsMessage_ = requirementsManager.Save("resources/tutorial_requirements.json")
			? "チュートリアル設定を保存しました。"
			: "チュートリアル設定を保存できませんでした。";
	}
	if (ImGui::Button("保存した設定を読み込む", ImVec2(-1.0f, 0.0f))) {
		tutorialRequirementsMessage_ = requirementsManager.Load("resources/tutorial_requirements.json")
			? "保存したチュートリアル設定を読み込みました。"
			: "チュートリアル設定を読み込めませんでした。";
	}
	if (ImGui::Button("初期値に戻して保存", ImVec2(-1.0f, 0.0f))) {
		requirementsManager.Reset();
		tutorialRequirementsMessage_ = requirementsManager.Save("resources/tutorial_requirements.json")
			? "初期値へ戻して保存しました。"
			: "初期値へ戻しましたが、保存できませんでした。";
	}
	if (!tutorialRequirementsMessage_.empty()) {
		ImGui::TextWrapped("%s", tutorialRequirementsMessage_.c_str());
	}
	ImGui::TextDisabled("保存先: resources/tutorial_requirements.json");
#else
	(void)requirementsManager;
#endif
}

void SystemSettingsPanel::DrawEnemyEventToolSection(
	EnemyEventManager& enemyEventManager,
	std::vector<EnemySpawnData>& enemySpawns) const {
#if defined(ENABLE_IMGUI)
	ImGui::Separator();
	ImGui::Text("イベントツール（増援設定）");

	static int selectedTrigger = 0;
	static int selectedTarget = 0;
	static int eventDelay = 60;

	std::vector<const char*> enemyNames;
	for (const auto& sp : enemySpawns) {
		enemyNames.push_back(sp.name.c_str());
	}

	if (!enemyNames.empty()) {
		if (selectedTrigger >= static_cast<int>(enemyNames.size())) selectedTrigger = 0;
		if (selectedTarget >= static_cast<int>(enemyNames.size())) selectedTarget = 0;

		ImGui::Combo("トリガーとなる敵", &selectedTrigger, enemyNames.data(), static_cast<int>(enemyNames.size()));
		ImGui::Combo("出現する敵(ターゲット)", &selectedTarget, enemyNames.data(), static_cast<int>(enemyNames.size()));
		ImGui::DragInt("出現までのディレイ(フレーム)", &eventDelay, 1, 0, 600);

		if (ImGui::Button("イベントを追加")) {
			enemyEventManager.AddEvent(enemyNames[selectedTrigger], enemyNames[selectedTarget], eventDelay);
			enemySpawns[selectedTarget].isInitialSpawn = false;
		}

		ImGui::SameLine();
		if (ImGui::Button("イベントを保存")) {
			enemyEventManager.SaveEvents("resources/enemy_events.json");
		}

		ImGui::Separator();
		ImGui::Text("設定済みのイベント一覧");
		for (size_t i = 0; i < enemyEventManager.GetEvents().size(); ++i) {
			const auto& ev = enemyEventManager.GetEvents()[i];
			ImGui::Text("[%d] %s が死んだら %d F後に %s が出現", (int)i, ev.triggerEnemyName.c_str(), ev.delayFrames, ev.targetEnemyName.c_str());
			ImGui::SameLine();
			if (ImGui::Button(("削除##" + std::to_string(i)).c_str())) {
				enemyEventManager.RemoveEvent(i);
				break;
			}
		}
	}
#else
	(void)enemyEventManager;
	(void)enemySpawns;
#endif
}

void SystemSettingsPanel::DrawAmmoSettingsSection(AmmoManager* ammoManager) {
#if defined(ENABLE_IMGUI)
	if (!ammoManager) {
		return;
	}

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
#else
	(void)ammoManager;
#endif
}
