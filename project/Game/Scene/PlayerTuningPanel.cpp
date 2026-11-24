#include "PlayerTuningPanel.h"

#include "Game/Player/BoosterEffect.h"
#include "Game/Player/Player.h"

#ifdef ENABLE_IMGUI
#include <externals/imgui/imgui.h>
#endif

#include <string>

void PlayerTuningPanel::DrawGameplayBalanceTab(const PlayerBalancePanelContext& context) {
#ifdef ENABLE_IMGUI
	if (!context.balanceManager) {
		return;
	}

	GameplayBalanceSettings balance = context.balanceManager->Get();
	GameplayBalanceMissileSettings missileSettings = context.missileSettings;
	bool changed = false;
	bool saveAfterEdit = false;
	auto editFloat = [&changed, &saveAfterEdit](const char* label, float* value, float min, float max, const char* format) {
		changed |= ImGui::SliderFloat(label, value, min, max, format, ImGuiSliderFlags_AlwaysClamp);
		saveAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
	};
	auto editInt = [&changed, &saveAfterEdit](const char* label, int* value, int min, int max) {
		changed |= ImGui::SliderInt(label, value, min, max, "%d", ImGuiSliderFlags_AlwaysClamp);
		saveAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
	};

	ImGui::TextWrapped("プレイヤー操作、武器、必殺技、歌、ボス増援のゲームプレイ数値をまとめて調整します。変更は即時反映され、操作を確定すると自動保存されます。");
	const char* modeNames[] = { "ファイター", "ガウォーク", "バトロイド" };
	for (int mode = 0; mode < 3; ++mode) {
		PlayerModeParams& params = balance.playerModes[mode];
		if (ImGui::TreeNode(modeNames[mode])) {
			ImGui::PushID(mode);
			editFloat("最大移動速度", &params.maxMoveSpeed, 0.01f, 1.0f, "%.3f");
			editFloat("移動加速度", &params.moveAcceleration, 0.001f, 0.10f, "%.3f");
			editFloat("移動減衰", &params.moveDamping, 0.10f, 1.0f, "%.2f");
			editFloat("ピッチ速度", &params.pitchSpeed, 0.001f, 0.10f, "%.3f");
			editFloat("ヨー速度", &params.yawSpeed, 0.001f, 0.10f, "%.3f");
			editFloat("ロール速度", &params.rollSpeed, 0.001f, 0.10f, "%.3f");
			changed |= ImGui::Checkbox("ガード可能", &params.canGuard); saveAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
			changed |= ImGui::Checkbox("近接攻撃可能", &params.canMelee); saveAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
			editInt("同時ロック数", &params.maxMultiLock, 1, 12);
			editFloat("ロック角度", &params.lockOnAngleDot, -1.0f, 1.0f, "%.2f");
			editFloat("ロック距離", &params.maxLockOnDistance, 1.0f, 1000.0f, "%.0f");
			ImGui::PopID();
			ImGui::TreePop();
		}
	}

	if (ImGui::TreeNode("通常弾")) {
		editFloat("速度##normal", &missileSettings.normalSpeed, 0.01f, 10.0f, "%.2f");
		editFloat("追尾力##normal", &missileSettings.normalHomingStrength, 0.0f, 1.0f, "%.2f");
		editFloat("表示サイズ##normal", &missileSettings.normalScale, 0.01f, 5.0f, "%.2f");
		editFloat("当たり判定半径##normal", &missileSettings.normalCollisionRadius, 0.01f, 10.0f, "%.2f");
		editInt("寿命フレーム##normal", &missileSettings.normalLifeTime, 1, 3600);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("誘導ミサイル")) {
		editFloat("速度##homing", &missileSettings.homingSpeed, 0.01f, 10.0f, "%.2f");
		editFloat("追尾力##homing", &missileSettings.homingStrength, 0.0f, 1.0f, "%.2f");
		editFloat("表示サイズ##homing", &missileSettings.homingScale, 0.01f, 5.0f, "%.2f");
		editFloat("当たり判定半径##homing", &missileSettings.homingCollisionRadius, 0.01f, 10.0f, "%.2f");
		editFloat("軌跡の太さ", &missileSettings.homingTrailWidth, 0.0f, 10.0f, "%.2f");
		editInt("寿命フレーム##homing", &missileSettings.homingLifeTime, 1, 3600);
		editFloat("発射位置オフセット", &missileSettings.muzzleOffset, 0.0f, 20.0f, "%.2f");
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("低空スピード線")) {
		const bool wasEnabled = balance.speedRushEnabled;
		changed |= ImGui::Checkbox("有効", &balance.speedRushEnabled);
		saveAfterEdit |= wasEnabled != balance.speedRushEnabled;
		ImGui::TextDisabled("ファイター形態で低空・高速時のみ、カメラ近傍を後方へ流れる大気・ダストです。");
		editFloat("最大高度", &balance.speedRushMaxAltitude, 1.0f, 200.0f, "%.1f");
		editFloat("発生開始速度比", &balance.speedRushMinSpeedRatio, 0.05f, 2.0f, "%.2f");
		editInt("最大発生数/フレーム", &balance.speedRushMaxParticlesPerFrame, 1, 16);
		editFloat("カメラ前方の発生距離", &balance.speedRushSpawnDistance, 1.0f, 50.0f, "%.1f");
		editFloat("横方向の発生幅", &balance.speedRushSpawnHalfWidth, 0.1f, 30.0f, "%.1f");
		editFloat("縦方向の発生幅", &balance.speedRushSpawnHalfHeight, 0.1f, 20.0f, "%.1f");
		editFloat("流れる速さ", &balance.speedRushParticleSpeed, 0.01f, 3.0f, "%.2f");
		editFloat("線のサイズ", &balance.speedRushParticleScale, 0.01f, 3.0f, "%.2f");
		editFloat("線の寿命", &balance.speedRushParticleLifetime, 0.05f, 2.0f, "%.2f 秒");
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("必殺技・歌・ボス")) {
		editFloat("必殺技SP消費", &balance.specialAttackCost, 0.0f, 100.0f, "%.1f");
		editFloat("SP回復/秒", &balance.spGaugeRecoveryPerSecond, 0.0f, 60.0f, "%.1f");
		editInt("必殺技時間フレーム", &balance.specialAttackDurationFrames, 1, 3600);
		editInt("必殺技発射間隔フレーム", &balance.specialAttackFireIntervalFrames, 1, 300);
		editInt("歌の継続フレーム", &balance.songDurationFrames, 60, 7200);
		editInt("ボス増援の同時上限", &balance.bossMaxActiveMinions, 0, 24);
		ImGui::TreePop();
	}

	if (changed) {
		context.balanceManager->Set(balance, context.player, &missileSettings);
		if (context.applyMissileSettings) {
			context.applyMissileSettings(missileSettings);
		}
	}
	if (saveAfterEdit && context.saveSettings) {
		gameplayBalanceMessage_ = context.saveSettings()
			? "ゲームバランス設定を保存して即時反映しました。" : "ゲームバランス設定を保存できませんでした。";
	}
	ImGui::Separator();
	if (ImGui::Button("ゲームバランスを保存", ImVec2(-1.0f, 0.0f))) {
		if (context.applyAllSettings) {
			context.applyAllSettings();
		}
		gameplayBalanceMessage_ = (context.saveSettings && context.saveSettings())
			? "ゲームバランス設定を保存しました。" : "ゲームバランス設定を保存できませんでした。";
	}
	if (ImGui::Button("保存したゲームバランスを読み込む", ImVec2(-1.0f, 0.0f))) {
		gameplayBalanceMessage_ = (context.loadSettings && context.loadSettings())
			? "ゲームバランス設定を読み込みました。" : "ゲームバランス設定を読み込めませんでした。";
	}
	if (!gameplayBalanceMessage_.empty()) {
		ImGui::TextWrapped("%s", gameplayBalanceMessage_.c_str());
	}
	ImGui::TextDisabled("保存先: resources/gameplay_balance_settings.json");
	ImGui::Separator();
	if (context.drawAmmoSettings) {
		context.drawAmmoSettings();
	}
#else
	(void)context;
#endif
}

void PlayerTuningPanel::DrawThrusterTuningTab(Player* player) {
#ifdef ENABLE_IMGUI
	ImGui::Text("スラスターの見た目・取付位置");
	ImGui::TextWrapped("炎の幅・長さ・明るさと取付位置を調整できます。変更はゲーム画面に即反映します。");
	ImGui::TextWrapped("ゲーム／シミュレーションの編集停止中も出力65%で噴射を表示します。通常・加速の見え方を切り替えて比較できます。");
	ImGui::Separator();
	if (!player || !player->GetBoosterEffect()) {
		ImGui::TextDisabled("プレイヤーが初期化されていません。");
		return;
	}

	BoosterEffect* booster = player->GetBoosterEffect();
	const auto saveAndApplySettings = [this, player, booster](const char* successMessage) {
		constexpr const char* kSettingsPath = "resources/vf-15c/thruster_position.json";
		if (!booster->SaveSettings(kSettingsPath)) {
			thrusterSettingsMessage_ = "保存できませんでした。resources/vf-15c に書き込み可能か確認してください。";
			return;
		}
		if (!booster->LoadSettings(kSettingsPath)) {
			thrusterSettingsMessage_ = "保存後の設定を読み込めませんでした。ファイル内容を確認してください。";
			return;
		}
		booster->RefreshPlacement(player->GetPosition(), player->GetQuaternion(),
			static_cast<int>(player->GetCurrentMode()));
		thrusterSettingsMessage_ = successMessage;
	};

	bool previewAccelerating = booster->GetEditorPreviewAcceleration();
	if (ImGui::Checkbox("加速状態をプレビュー", &previewAccelerating)) {
		booster->SetEditorPreviewAcceleration(previewAccelerating);
	}
	bool thrusterPlumeEnabled = booster->IsThrusterPlumeEnabled();
	if (ImGui::Checkbox("光跡を表示", &thrusterPlumeEnabled)) {
		booster->SetThrusterPlumeEnabled(thrusterPlumeEnabled);
		saveAndApplySettings("光跡の表示設定を保存して、ただちに反映しました。");
	}
	ImGui::TextDisabled("OFFにすると、ノズル本体は残して光跡と炎だけを非表示にします。");
	ImGui::Separator();
	ImGui::Text("エフェクトの見た目");
	BoosterEffect::EffectSettings effect = booster->GetEffectSettings();
	bool effectChanged = false;
	bool saveEffectSettings = false;
	const auto editEffectSlider = [&effectChanged, &saveEffectSettings](const char* label, float* value,
		float minimum, float maximum, const char* format) {
		effectChanged |= ImGui::SliderFloat(label, value, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
		// ドラッグ中に毎フレーム書き込まず、操作を終えた時点で保存対象にする。
		saveEffectSettings |= ImGui::IsItemDeactivatedAfterEdit();
	};
	editEffectSlider("炎の幅", &effect.widthScale, 0.25f, 6.0f, "%.2f 倍");
	editEffectSlider("炎の長さ", &effect.lengthScale, 0.25f, 4.0f, "%.2f 倍");
	editEffectSlider("明るさ", &effect.brightness, 0.1f, 3.0f, "%.2f 倍");
	editEffectSlider("周囲の光", &effect.glowScale, 0.0f, 3.0f, "%.2f 倍");
	if (ImGui::TreeNode("加速・白い芯の調整")) {
		editEffectSlider("加速時の幅倍率", &effect.boostWidthMultiplier, 1.0f, 2.0f, "%.2f 倍");
		editEffectSlider("加速時の長さ倍率", &effect.boostLengthMultiplier, 1.0f, 2.0f, "%.2f 倍");
		editEffectSlider("白い芯の長さ比率", &effect.coreLengthRatio, 0.2f, 0.8f, "%.2f");
		ImGui::TreePop();
	}
	if (effectChanged) {
		booster->SetEffectSettings(effect);
	}
	if (ImGui::Button("エフェクトの見た目をリセット", ImVec2(-1.0f, 0.0f))) {
		booster->ResetEffectSettings();
		saveEffectSettings = true;
	}
	if (saveEffectSettings) {
		saveAndApplySettings("エフェクト設定を保存して、ただちに反映しました。");
	}
	ImGui::TextDisabled("エフェクトの変更は、操作を終えると保存・即時反映されます。");
	ImGui::Separator();
	const int placementMode = static_cast<int>(player->GetCurrentMode());
	const char* placementModeName = placementMode == 0 ? "ファイター"
		: (placementMode == 1 ? "ガウォーク" : "バトロイド");
	ImGui::Text("取付調整（現在の形態: %s）", placementModeName);
	ImGui::TextDisabled("形態ごとに位置・サイズ・回転を別々に保存できます。");
	ImGui::Checkbox("位置合わせ用マーカー", &showThrusterPlacementMarkers_);
	booster->SetPlacementMarkersVisible(showThrusterPlacementMarkers_);
	BoosterEffect::PlacementSettings placement = booster->GetPlacementSettings(placementMode);
	bool placementChanged = false;
	placementChanged |= ImGui::DragFloat3("位置補正 X・Y・Z", &placement.positionOffset.x,
		0.01f, -3.0f, 3.0f, "%.3f m");
	placementChanged |= ImGui::DragFloat3("ノズルサイズ X・Y・Z", &placement.scale.x,
		0.01f, 0.2f, 3.0f, "%.2f 倍");
	placementChanged |= ImGui::DragFloat3("ノズル回転 X・Y・Z", &placement.rotationDegrees.x,
		1.0f, -180.0f, 180.0f, "%.1f 度");
	if (placementChanged) {
		booster->SetPlacementSettings(placementMode, placement);
		booster->RefreshPlacement(player->GetPosition(), player->GetQuaternion(), placementMode);
	}
	ImGui::TextDisabled("+X: 右 / +Y: 上 / +Z: 前方。Yをマイナスにすると下がります。");
	ImGui::TextDisabled("回転は機体ローカル軸、サイズはノズルと噴射炎に反映されます。");
	if (ImGui::Button("現在の形態の取付調整をリセット", ImVec2(-1.0f, 0.0f))) {
		booster->ResetPlacementAdjustment(placementMode);
		booster->RefreshPlacement(player->GetPosition(), player->GetQuaternion(), placementMode);
	}
	ImGui::Separator();
	ImGui::Text("追加スラスター（現在の形態のみ）");
	ImGui::TextDisabled("追加したノズルは、位置・サイズ・回転を1基ずつ編集して保存できます。最大8基です。");
	const auto& extraThrusters = booster->GetExtraThrusters(placementMode);
	bool extraThrusterChanged = false;
	bool removeExtraThruster = false;
	size_t extraThrusterToRemove = 0;
	for (size_t index = 0; index < extraThrusters.size(); ++index) {
		BoosterEffect::ExtraThrusterSettings extra = extraThrusters[index];
		bool extraChanged = false;
		ImGui::PushID(static_cast<int>(index));
		const std::string title = "追加ノズル " + std::to_string(index + 1);
		if (ImGui::TreeNode(title.c_str())) {
			extraChanged |= ImGui::DragFloat3("位置 X・Y・Z", &extra.position.x,
				0.01f, -3.0f, 3.0f, "%.3f m");
			extraChanged |= ImGui::DragFloat3("サイズ X・Y・Z", &extra.scale.x,
				0.01f, 0.2f, 3.0f, "%.2f 倍");
			extraChanged |= ImGui::DragFloat3("回転 X・Y・Z", &extra.rotationDegrees.x,
				1.0f, -180.0f, 180.0f, "%.1f 度");
			if (ImGui::Button("このノズルを削除")) {
				removeExtraThruster = true;
				extraThrusterToRemove = index;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
		if (extraChanged) {
			booster->SetExtraThrusterSettings(placementMode, index, extra);
			extraThrusterChanged = true;
		}
	}
	if (removeExtraThruster) {
		booster->RemoveExtraThruster(placementMode, extraThrusterToRemove);
		extraThrusterChanged = true;
	}
	if (ImGui::Button("スラスターを追加", ImVec2(-1.0f, 0.0f))) {
		if (booster->AddExtraThruster(placementMode)) {
			extraThrusterChanged = true;
		} else {
			thrusterSettingsMessage_ = "追加できるスラスターは1形態につき8基までです。";
		}
	}
	if (extraThrusterChanged) {
		booster->RefreshPlacement(player->GetPosition(), player->GetQuaternion(), placementMode);
	}
	if (ImGui::Button("位置・エフェクト設定を保存して即時反映", ImVec2(-1.0f, 0.0f))) {
		saveAndApplySettings("取付位置・追加スラスター・エフェクト設定を保存して、ただちに反映しました。");
	}
	if (ImGui::Button("保存設定を読み込む", ImVec2(-1.0f, 0.0f))) {
		if (booster->LoadSettings("resources/vf-15c/thruster_position.json")) {
			booster->RefreshPlacement(player->GetPosition(), player->GetQuaternion(),
				static_cast<int>(player->GetCurrentMode()));
			thrusterSettingsMessage_ = "取付位置とエフェクト設定を読み込みました。";
		} else {
			thrusterSettingsMessage_ = "保存データを読み込めません。先に設定を保存するか、ファイル内容を確認してください。";
		}
	}
	if (!thrusterSettingsMessage_.empty()) {
		ImGui::TextWrapped("%s", thrusterSettingsMessage_.c_str());
	}
	ImGui::TextDisabled("保存先: resources/vf-15c/thruster_position.json");
#else
	(void)player;
#endif
}
