#include "EffectTuningPanel.h"

#include "BoundaryShieldSettingsManager.h"
#include "Game/Player/Player.h"
#include "engine/Camera/Camera.h"
#include "engine/Particle/ExplosionManager.h"
#include <externals/imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

void EffectTuningPanel::UpdatePreviewTimer(float deltaTime) {
	if (destructionEffectPreviewFrames_ > 0) {
		--destructionEffectPreviewFrames_;
		destructionEffectPreviewElapsed_ += deltaTime;
	}
}

void EffectTuningPanel::DrawDestructionEffectTab(
	ExplosionManager* explosionManager,
	const Camera* camera,
	const Player* player) {
#if defined(ENABLE_IMGUI)
	if (!explosionManager) {
		return;
	}

	const char* targetLabels[] = {
		"通常敵（飛行）", "地上敵", "ジャマー敵", "ボス", "プレイヤー"
	};
	destructionEffectTargetIndex_ = (std::clamp)(
		destructionEffectTargetIndex_, 0, IM_ARRAYSIZE(targetLabels) - 1);
	ImGui::Text("対象別 撃破パーティクル");
	ImGui::SetNextItemWidth(210.0f);
	ImGui::Combo("対象敵", &destructionEffectTargetIndex_, targetLabels, IM_ARRAYSIZE(targetLabels));

	const auto target = static_cast<ExplosionManager::EffectTarget>(destructionEffectTargetIndex_);
	if (target == ExplosionManager::EffectTarget::StandardEnemy) {
		ImGui::TextDisabled("通常敵の設定は共通設定の基準になります。");
	} else {
		bool useCommonConfig = explosionManager->UsesCommonConfig(target);
		if (ImGui::Checkbox("通常敵と同じ設定を使用", &useCommonConfig)) {
			explosionManager->SetUseCommonConfig(target, useCommonConfig);
		}
		ImGui::TextDisabled(useCommonConfig
			? "共通設定を参照中です。チェックを外すとこの対象専用に編集できます。"
			: "この対象専用の設定を編集中です。");
	}

	auto& config = explosionManager->GetConfig(target);
	auto applyQuickPreset = [&](ExplosionManager::DestructionParticleType particleType,
		int count, float scale, float speed, float lifeTime, const float (&color)[4]) {
		config.destructionParticleType = static_cast<int>(particleType);
		config.count = count;
		config.scale = scale;
		config.speed = speed;
		config.lifeTimeMin = lifeTime * 0.58f;
		config.lifeTimeMax = lifeTime;
		for (int i = 0; i < 4; ++i) {
			config.color[i] = color[i];
		}
	};
	auto getPreviewPosition = [&]() {
		// 敵の実位置では遠方や地形の裏に隠れるため、常に表示中のカメラ正面で確認する。
		if (camera) {
			const Matrix4x4& world = camera->GetWorldMatrix();
			Vector3 forward{ world.m[2][0], world.m[2][1], world.m[2][2] };
			const float length = std::sqrt(
				forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
			if (length > 0.0001f) {
				forward.x /= length;
				forward.y /= length;
				forward.z /= length;
				const Vector3& cameraPosition = camera->GetTranslate();
				return Vector3{
					cameraPosition.x + forward.x * destructionEffectPreviewDistance_,
					cameraPosition.y + forward.y * destructionEffectPreviewDistance_,
					cameraPosition.z + forward.z * destructionEffectPreviewDistance_,
				};
			}
		}
		return player ? player->GetPosition() : Vector3{ 0.0f, 3.0f, 20.0f };
	};
	auto playPreview = [&]() {
		if (destructionEffectClearBeforePreview_) {
			explosionManager->ClearEffects();
		}
		explosionManager->CreateDestructionPreview(
			{ getPreviewPosition() }, target, destructionEffectPreviewShowShockwave_);
		destructionEffectPreviewTimer_ = 0.0f;
		destructionEffectPreviewElapsed_ = 0.0f;
		destructionEffectPreviewDuration_ = (std::max)(0.7f, config.lifeTimeMax);
		destructionEffectPreviewFrames_ = (std::max)(60,
			static_cast<int>(std::ceil((destructionEffectPreviewDuration_ + 0.35f) * 60.0f)));
	};

	bool configEdited = false;
	auto noteEdit = [&]() {
		configEdited |= ImGui::IsItemDeactivatedAfterEdit();
	};
	const float smallPresetColor[4] = { 1.0f, 0.33f, 0.04f, 0.95f };
	const float largePresetColor[4] = { 1.0f, 0.58f, 0.10f, 1.0f };
	const float sparkPresetColor[4] = { 1.0f, 0.78f, 0.25f, 1.0f };

	ImGui::Separator();
	if (ImGui::CollapsingHeader("クイックプリセット", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("小型機 撃墜", ImVec2(100.0f, 28.0f))) {
			applyQuickPreset(ExplosionManager::DestructionParticleType::FlameToSmoke, 48, 0.32f, 0.06f, 0.65f, smallPresetColor);
			config.flameToSmokeScale = 2.8f;
			configEdited = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("大型爆発", ImVec2(100.0f, 28.0f))) {
			applyQuickPreset(ExplosionManager::DestructionParticleType::FlameToSmoke, 132, 0.58f, 0.12f, 1.15f, largePresetColor);
			config.flameToSmokeScale = 3.6f;
			configEdited = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("火花散乱", ImVec2(100.0f, 28.0f))) {
			applyQuickPreset(ExplosionManager::DestructionParticleType::Sparks, 96, 0.11f, 0.18f, 0.50f, sparkPresetColor);
			configEdited = true;
		}
		if (ImGui::Button("煙雲", ImVec2(100.0f, 28.0f))) {
			const float smokePresetColor[4] = { 0.18f, 0.18f, 0.20f, 0.72f };
			applyQuickPreset(ExplosionManager::DestructionParticleType::SmokeCloud, 52, 0.46f, 0.08f, 1.45f, smokePresetColor);
			for (int i = 0; i < 4; ++i) {
				config.smokeColor[i] = smokePresetColor[i];
			}
			config.flameToSmokeScale = 3.2f;
			configEdited = true;
		}
		ImGui::TextDisabled("現在選択中の対象だけへ適用します。");
	}

	if (ImGui::CollapsingHeader("パーティクルと見た目", ImGuiTreeNodeFlags_DefaultOpen)) {
		const char* destructionParticleTypes[] = {
			"炎 → 煙（推奨）", "火球", "煙雲", "火花"
		};
		config.destructionParticleType = (std::clamp)(
			config.destructionParticleType, 0, IM_ARRAYSIZE(destructionParticleTypes) - 1);
		ImGui::Combo("パーティクル種類", &config.destructionParticleType,
			destructionParticleTypes, IM_ARRAYSIZE(destructionParticleTypes));
		noteEdit();
		ImGui::DragInt("発生数", &config.count, 1, 0, 1000);
		noteEdit();
		ImGui::ColorEdit4("炎カラー", config.color);
		noteEdit();
		if (config.destructionParticleType == static_cast<int>(ExplosionManager::DestructionParticleType::FlameToSmoke)) {
			ImGui::ColorEdit4("煙カラー", config.smokeColor);
			noteEdit();
			ImGui::DragFloat("煙への変化開始", &config.flameToSmokeTransition, 0.01f, 0.0f, 0.95f);
			noteEdit();
			ImGui::DragFloat("最終スケール倍率", &config.flameToSmokeScale, 0.05f, 1.0f, 8.0f);
			noteEdit();
		}
	}

	if (ImGui::CollapsingHeader("発生・動き・寿命", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("速度", &config.speed, 0.01f, 0.0f, 10.0f); noteEdit();
		ImGui::DragFloat("速度ばらつき", &config.speedVariance, 0.01f, 0.0f, 5.0f); noteEdit();
		ImGui::DragFloat("スケール", &config.scale, 0.001f, 0.0f, 5.0f); noteEdit();
		ImGui::DragFloat("スケールばらつき", &config.scaleVariance, 0.001f, 0.0f, 2.0f); noteEdit();
		ImGui::DragFloat("最小寿命", &config.lifeTimeMin, 0.01f, 0.0f, 10.0f); noteEdit();
		ImGui::DragFloat("最大寿命", &config.lifeTimeMax, 0.01f, 0.0f, 10.0f); noteEdit();
		ImGui::DragFloat("位置ばらつき", &config.posVariance, 0.01f, 0.0f, 5.0f); noteEdit();
	}

	if (ImGui::CollapsingHeader("プレビュー", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("カメラ前の距離", &destructionEffectPreviewDistance_, 0.1f, 2.0f, 80.0f);
		ImGui::Checkbox("再生前に前回のエフェクトを消す", &destructionEffectClearBeforePreview_);
		ImGui::Checkbox("値変更時に自動再生", &destructionEffectAutoPreview_);
		if (ImGui::Checkbox("3D衝撃波リングをプレビューする", &destructionEffectPreviewShowShockwave_)) {
			// 非表示へ切り替えた瞬間も、画面を覆っている既存リングを取り除く。
			if (!destructionEffectPreviewShowShockwave_) {
				explosionManager->ClearEffects();
			}
		}
		ImGui::Checkbox("ループ再生", &destructionEffectPreviewLoop_);
		if (destructionEffectPreviewLoop_) {
			ImGui::DragFloat("ループ間隔", &destructionEffectPreviewLoopInterval_, 0.05f, 0.2f, 10.0f);
		}
		if (ImGui::Button("▶ 1回再生", ImVec2(120.0f, 28.0f))) {
			playPreview();
		}
		ImGui::SameLine();
		if (ImGui::Button("■ 停止して消去", ImVec2(120.0f, 28.0f))) {
			explosionManager->ClearEffects();
			destructionEffectPreviewLoop_ = false;
			destructionEffectPreviewFrames_ = 0;
			destructionEffectPreviewElapsed_ = 0.0f;
		}
		ImGui::Text("状態: %s  |  Active: %zu", destructionEffectPreviewFrames_ > 0 ? "再生中" : "待機中",
			explosionManager->GetActiveEffectCount());
		const float progress = destructionEffectPreviewDuration_ > 0.0f
			? (std::min)(1.0f, destructionEffectPreviewElapsed_ / destructionEffectPreviewDuration_) : 0.0f;
		char timelineLabel[64]{};
		std::snprintf(timelineLabel, sizeof(timelineLabel), "%.2f / %.2f s", destructionEffectPreviewElapsed_, destructionEffectPreviewDuration_);
		ImGui::ProgressBar(progress, ImVec2(-1.0f, 18.0f), timelineLabel);
		ImGui::TextDisabled("炎  0.0s ── 変化 %.2fs ── 煙  %.2fs", config.flameToSmokeTransition * destructionEffectPreviewDuration_, destructionEffectPreviewDuration_);
		ImGui::TextDisabled("表示中のカメラ正面で再生します。編集停止中でも動作します。");
	}

	if (configEdited && destructionEffectAutoPreview_) {
		playPreview();
	}
	if (destructionEffectPreviewLoop_) {
		destructionEffectPreviewTimer_ += ImGui::GetIO().DeltaTime;
		if (destructionEffectPreviewTimer_ >= destructionEffectPreviewLoopInterval_) {
			playPreview();
		}
	}

	ImGui::Separator();
	if (ImGui::Button("設定をJSONに保存")) {
		explosionManager->SaveToJson("resources/explosionConfig.json");
	}
	ImGui::SameLine();
	if (ImGui::Button("設定をJSONから読込")) {
		explosionManager->LoadFromJson("resources/explosionConfig.json");
	}
#else
	(void)explosionManager;
	(void)camera;
	(void)player;
#endif
}

void EffectTuningPanel::DrawBoundaryShieldTab(
	BoundaryShieldSettingsManager& boundaryShieldSettings,
	Player* player) {
#if defined(ENABLE_IMGUI)
	auto settings = boundaryShieldSettings.Get();
	bool changed = false;
	bool saveAfterEdit = false;
	auto editFloat = [&changed, &saveAfterEdit](const char* label, float* value, float minimum, float maximum, const char* format) {
		changed |= ImGui::SliderFloat(label, value, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
		saveAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
	};

	ImGui::Text("空中に表示する六角形エネルギーシールド");
	ImGui::TextDisabled("数値は変更を確定すると即時反映・自動保存されます。");
	ImGui::Separator();
	editFloat("警告を開始する距離", &settings.warningDistance, 5.0f, 250.0f, "%.1f");
	editFloat("壁シールド幅", &settings.wallWidth, 1.0f, 100.0f, "%.1f");
	editFloat("壁シールド高さ", &settings.wallHeight, 1.0f, 100.0f, "%.1f");
	editFloat("天井シールド幅", &settings.ceilingWidth, 1.0f, 100.0f, "%.1f");
	editFloat("天井シールド高さ", &settings.ceilingHeight, 1.0f, 100.0f, "%.1f");
	editFloat("最大不透明度", &settings.maxOpacity, 0.05f, 1.0f, "%.2f");
	editFloat("脈動速度", &settings.pulseSpeed, 0.0f, 0.50f, "%.3f");
	editFloat("脈動の強さ", &settings.pulseStrength, 0.0f, 0.50f, "%.2f");
	editFloat("境界面からのオフセット", &settings.surfaceOffset, 0.0f, 5.0f, "%.2f");
	float shieldColor[3] = { settings.color.x, settings.color.y, settings.color.z };
	if (ImGui::ColorEdit3("シールドカラー", shieldColor, ImGuiColorEditFlags_Float)) {
		settings.color = { shieldColor[0], shieldColor[1], shieldColor[2] };
		changed = true;
	}
	saveAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();

	if (changed) {
		boundaryShieldSettings.Set(settings, player);
	}
	if (saveAfterEdit) {
		boundaryShieldSettingsMessage_ = boundaryShieldSettings.Save("resources/boundary_shield_settings.json")
			? "境界シールド設定を保存して即時反映しました。"
			: "境界シールド設定を保存できませんでした。";
	}

	ImGui::Separator();
	if (ImGui::Button("現在の設定を保存", ImVec2(-1.0f, 0.0f))) {
		boundaryShieldSettings.Set(settings, player);
		boundaryShieldSettingsMessage_ = boundaryShieldSettings.Save("resources/boundary_shield_settings.json")
			? "境界シールド設定を保存しました。"
			: "境界シールド設定を保存できませんでした。";
	}
	if (ImGui::Button("保存設定を読み込む", ImVec2(-1.0f, 0.0f))) {
		boundaryShieldSettingsMessage_ = boundaryShieldSettings.Load("resources/boundary_shield_settings.json", player)
			? "境界シールド設定を読み込み、即時反映しました。"
			: "境界シールド設定を読み込めませんでした。";
	}
	if (ImGui::Button("初期値に戻す", ImVec2(-1.0f, 0.0f))) {
		boundaryShieldSettings.Reset(player);
		boundaryShieldSettingsMessage_ = boundaryShieldSettings.Save("resources/boundary_shield_settings.json")
			? "初期値へ戻して保存しました。"
			: "初期値へ戻しましたが保存できませんでした。";
	}
	if (!boundaryShieldSettingsMessage_.empty()) {
		ImGui::TextWrapped("%s", boundaryShieldSettingsMessage_.c_str());
	}
	ImGui::TextDisabled("保存先: resources/boundary_shield_settings.json");
#else
	(void)boundaryShieldSettings;
	(void)player;
#endif
}
