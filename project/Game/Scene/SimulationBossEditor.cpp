#include "SimulationBossEditor.h"

#include "BossEncounterState.h"
#include "BossSpawnController.h"
#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h"
#include "GameOverSequence.h"
#include "externals/json.hpp"

#include <Windows.h>
#include <algorithm>
#include <exception>
#include <externals/imgui/imgui.h>
#include <fstream>
#include <vector>

using json = nlohmann::json;

namespace {
	Boss* FindActiveBoss(const std::list<std::unique_ptr<Enemy>>* enemies) {
		if (!enemies) {
			return nullptr;
		}
		for (const auto& enemy : *enemies) {
			if (auto* boss = dynamic_cast<Boss*>(enemy.get()); boss && !boss->IsDead()) {
				return boss;
			}
		}
		return nullptr;
	}
}

bool SimulationBossEditor::SaveSettings(const std::string& filePath, const SimulationBossEditorContext& context) {
	if (!context.bossBattlePosition || !context.bossTuning) {
		return false;
	}
	auto setStatus = [&context](const std::string& msg) {
		if (context.statusMessage) {
			*context.statusMessage = msg;
		}
	};

	// 出現中のボスがいる場合は、最新のボスの位置とチューニングを即座に取り込む
	if (Boss* boss = FindActiveBoss(context.enemies)) {
		const Vector3 pos = boss->GetPosition();
		context.bossBattlePosition[0] = pos.x;
		context.bossBattlePosition[1] = pos.y;
		context.bossBattlePosition[2] = pos.z;
		*context.bossTuning = boss->GetTuning();
	}

	const Boss::Tuning& tuning = *context.bossTuning;
	json offsets = json::array();
	for (const Vector3& offset : tuning.rearThrusterIndividualOffsets) {
		offsets.push_back({ offset.x, offset.y, offset.z });
	}
	json root = {
		{ "version", 1 },
		{ "bossPosition", {
			context.bossBattlePosition[0],
			context.bossBattlePosition[1],
			context.bossBattlePosition[2],
		} },
		{ "tuning", {
			{ "maxHP", tuning.maxHP },
			{ "mainTurretYawTracking", tuning.mainTurretYawTracking },
			{ "mainTurretPitchTracking", tuning.mainTurretPitchTracking },
			{ "mainTurretYawFollow", tuning.mainTurretYawFollow },
			{ "mainTurretPitchFollow", tuning.mainTurretPitchFollow },
			{ "mainTurretMaxPitch", tuning.mainTurretMaxPitch },
			{ "forwardTurretsVerticalSlide", tuning.forwardTurretsVerticalSlide },
			{ "forwardTurretSlideAmount", tuning.forwardTurretSlideAmount },
			{ "forwardTurretSlideSpeed", tuning.forwardTurretSlideSpeed },
			{ "forwardTurretAimSlide", tuning.forwardTurretAimSlide },
			{ "rearThrusterCount", tuning.rearThrusterCount },
			{ "rearThrusterOffset", { tuning.rearThrusterOffset.x, tuning.rearThrusterOffset.y, tuning.rearThrusterOffset.z } },
			{ "rearThrusterSpacing", tuning.rearThrusterSpacing },
			{ "rearThrusterRadius", tuning.rearThrusterRadius },
			{ "rearThrusterLength", tuning.rearThrusterLength },
			{ "rearThrusterBrightness", tuning.rearThrusterBrightness },
			{ "rearThrusterFlareRadius", tuning.rearThrusterFlareRadius },
			{ "rearThrusterIndividualOffsets", offsets },
			{ "sideShieldOpacity", tuning.sideShieldOpacity },
		} },
	};

	const std::string dumpStr = root.dump(4);
	std::ofstream ofs(filePath, std::ios::trunc);
	if (!ofs.is_open()) {
		setStatus("ボスの位置・設定を保存できませんでした。");
		return false;
	}
	ofs << dumpStr;

	// プロジェクトソース側の resources フォルダが存在していれば、クリーンビルド後も保持されるよう保存する
	const std::vector<std::string> secondaryPaths = {
		"project/resources/boss_thruster_settings.json",
		"../project/resources/boss_thruster_settings.json",
	};
	for (const auto& secPath : secondaryPaths) {
		std::ofstream secOfs(secPath, std::ios::trunc);
		if (secOfs.is_open()) {
			secOfs << dumpStr;
		}
	}

	setStatus("ボスの位置・設定を保存しました。");
	return true;
}

bool SimulationBossEditor::LoadSettings(
	const std::string& filePath, bool verbose, const SimulationBossEditorContext& context) {
	if (!context.bossBattlePosition || !context.bossTuning) {
		return false;
	}
	auto setStatus = [&context, verbose](const std::string& msg) {
		if (verbose && context.statusMessage) {
			*context.statusMessage = msg;
		}
	};

	std::ifstream ifs(filePath);
	if (!ifs.is_open()) {
		// カレントにない場合はプロジェクト側も検索
		const std::vector<std::string> fallbackPaths = {
			"project/resources/boss_thruster_settings.json",
			"../project/resources/boss_thruster_settings.json",
		};
		for (const auto& fbPath : fallbackPaths) {
			ifs.open(fbPath);
			if (ifs.is_open()) break;
		}
		if (!ifs.is_open()) {
			setStatus("保存済みのボス設定がありません。");
			return false;
		}
	}

	json root;
	try {
		ifs >> root;
	} catch (const std::exception& e) {
		OutputDebugStringA((std::string("[SimulationBossEditor] Failed to load boss settings JSON: ") + e.what() + "\n").c_str());
		setStatus("ボス設定の読み込みに失敗しました。");
		return false;
	}
	if (!root.is_object() || !root.contains("tuning") || !root["tuning"].is_object()) {
		setStatus("ボス設定の形式が正しくありません。");
		return false;
	}

	Boss::Tuning tuning = *context.bossTuning;
	const json& data = root["tuning"];
	tuning.maxHP = std::clamp(data.value("maxHP", Boss::kMaxHP), 1, 1000000);
	tuning.mainTurretYawTracking = data.value("mainTurretYawTracking", tuning.mainTurretYawTracking);
	tuning.mainTurretPitchTracking = data.value("mainTurretPitchTracking", tuning.mainTurretPitchTracking);
	tuning.mainTurretYawFollow = data.value("mainTurretYawFollow", tuning.mainTurretYawFollow);
	tuning.mainTurretPitchFollow = data.value("mainTurretPitchFollow", tuning.mainTurretPitchFollow);
	tuning.mainTurretMaxPitch = data.value("mainTurretMaxPitch", tuning.mainTurretMaxPitch);
	tuning.forwardTurretsVerticalSlide = data.value("forwardTurretsVerticalSlide", tuning.forwardTurretsVerticalSlide);
	tuning.forwardTurretSlideAmount = data.value("forwardTurretSlideAmount", tuning.forwardTurretSlideAmount);
	tuning.forwardTurretSlideSpeed = data.value("forwardTurretSlideSpeed", tuning.forwardTurretSlideSpeed);
	tuning.forwardTurretAimSlide = data.value("forwardTurretAimSlide", tuning.forwardTurretAimSlide);
	tuning.rearThrusterCount = data.value("rearThrusterCount", tuning.rearThrusterCount);
	tuning.rearThrusterSpacing = data.value("rearThrusterSpacing", tuning.rearThrusterSpacing);
	tuning.rearThrusterRadius = data.value("rearThrusterRadius", tuning.rearThrusterRadius);
	tuning.rearThrusterLength = data.value("rearThrusterLength", tuning.rearThrusterLength);
	tuning.rearThrusterBrightness = data.value("rearThrusterBrightness", tuning.rearThrusterBrightness);
	tuning.rearThrusterFlareRadius = data.value("rearThrusterFlareRadius", tuning.rearThrusterFlareRadius);
	tuning.sideShieldOpacity = data.value("sideShieldOpacity", tuning.sideShieldOpacity);
	if (data.contains("rearThrusterOffset") && data["rearThrusterOffset"].is_array() && data["rearThrusterOffset"].size() >= 3) {
		tuning.rearThrusterOffset = {
			data["rearThrusterOffset"][0].get<float>(),
			data["rearThrusterOffset"][1].get<float>(),
			data["rearThrusterOffset"][2].get<float>()
		};
	}
	if (data.contains("rearThrusterIndividualOffsets") && data["rearThrusterIndividualOffsets"].is_array()) {
		const json& offsets = data["rearThrusterIndividualOffsets"];
		for (size_t index = 0; index < tuning.rearThrusterIndividualOffsets.size() && index < offsets.size(); ++index) {
			if (offsets[index].is_array() && offsets[index].size() >= 3) {
				tuning.rearThrusterIndividualOffsets[index] = {
					offsets[index][0].get<float>(),
					offsets[index][1].get<float>(),
					offsets[index][2].get<float>()
				};
			}
		}
	}

	if (root.contains("bossPosition") && root["bossPosition"].is_array() && root["bossPosition"].size() >= 3) {
		context.bossBattlePosition[0] = root["bossPosition"][0].get<float>();
		context.bossBattlePosition[1] = root["bossPosition"][1].get<float>();
		context.bossBattlePosition[2] = root["bossPosition"][2].get<float>();
	}
	*context.bossTuning = tuning;
	if (Boss* boss = FindActiveBoss(context.enemies)) {
		boss->SetTuning(tuning);
		boss->SetPosition({ context.bossBattlePosition[0], context.bossBattlePosition[1], context.bossBattlePosition[2] });
		boss->UpdateModel();
	}
	setStatus("保存したボスの位置・設定を読み込みました。");
	return true;
}

void SimulationBossEditor::DrawControls(const SimulationBossEditorContext& context) {
#ifdef ENABLE_IMGUI
	if (!context.bossBattlePosition || !context.bossTuning) {
		return;
	}
	auto setStatus = [&context](const std::string& msg) {
		if (context.statusMessage) {
			*context.statusMessage = msg;
		}
	};

	ImGui::Text("=== ボス戦テスト / ボス調整 ===");
	ImGui::TextDisabled("開始すると現在の敵を入れ替え、指定位置にボスを1体だけ出現させます。");
	ImGui::DragFloat3("ボス出現座標 (X,Y,Z)##boss", context.bossBattlePosition, 0.25f, -100.0f, 100.0f, "%.2f");
	ImGui::SameLine();
	if (ImGui::Button("座標を保存##saveBossPosQuick")) {
		SaveSettings("resources/boss_thruster_settings.json", context);
	}

	Boss* activeBoss = FindActiveBoss(context.enemies);
	if (ImGui::Button("この位置でボス戦を開始##boss") && context.enemies && context.obstacles) {
		if (context.clearAllEnemyReferences) {
			context.clearAllEnemyReferences();
		}
		context.enemies->clear();
		BossSpawnController bossSpawner;
		bossSpawner.Spawn(
			{ context.bossBattlePosition[0], context.bossBattlePosition[1], context.bossBattlePosition[2] },
			context.bossTuning,
			*context.obstacles,
			*context.enemies);
		if (context.bossEncounterState) context.bossEncounterState->MarkBossSpawned();
		if (context.gameOverSequence) context.gameOverSequence->Reset();
		if (context.isEditorPreviewPlaying) *context.isEditorPreviewPlaying = true;
		setStatus("ボス戦を開始しました。再生中のまま砲台とスラスターを調整できます。");
		activeBoss = FindActiveBoss(context.enemies);
	}
	ImGui::SameLine();
	if (ImGui::Button("現在のボス位置を取得##boss") && activeBoss) {
		const Vector3 position = activeBoss->GetPosition();
		context.bossBattlePosition[0] = position.x;
		context.bossBattlePosition[1] = position.y;
		context.bossBattlePosition[2] = position.z;
	}
	if (activeBoss) {
		if (ImGui::Button("設定した座標へボスを移動##boss")) {
			activeBoss->SetPosition({ context.bossBattlePosition[0], context.bossBattlePosition[1], context.bossBattlePosition[2] });
			activeBoss->UpdateModel();
		}
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.45f, 1.0f), "ボス出現中  HP: %d/%d", activeBoss->GetHP(), activeBoss->GetMaxHP());
		if (ImGui::Button("ボスを即時撃破##boss")) {
			// 通常のダメージ経路を通すことで、ゲーム側の撃破演出・クリア処理も同じように検証できる。
			activeBoss->TakeDamage(activeBoss->GetHP());
			setStatus("ボスを即時撃破しました。通常ゲームではクリア演出が始まります。");
		}
		ImGui::TextDisabled("ボス本体を即時撃破し、撃破演出を確認します。");
	}

	bool changed = false;
	Boss::Tuning& tuning = *context.bossTuning;
	if (ImGui::InputInt("ボス最大HP##boss", &tuning.maxHP)) {
		tuning.maxHP = std::clamp(tuning.maxHP, 1, 1000000);
		changed = true;
	}
	ImGui::TextDisabled("HP変更時は出現中のボスも満タンに更新します。下の保存ボタンで次回起動にも反映します。");
	if (ImGui::CollapsingHeader("砲台の回転・スライド##boss", ImGuiTreeNodeFlags_DefaultOpen)) {
		changed |= ImGui::Checkbox("主砲の横追尾 (ヨー)##boss", &tuning.mainTurretYawTracking);
		changed |= ImGui::Checkbox("主砲の縦追尾 (ピッチ)##boss", &tuning.mainTurretPitchTracking);
		changed |= ImGui::SliderFloat("主砲 横追尾の速さ##boss", &tuning.mainTurretYawFollow, 0.0f, 0.35f, "%.3f");
		changed |= ImGui::SliderFloat("主砲 縦追尾の速さ##boss", &tuning.mainTurretPitchFollow, 0.0f, 0.35f, "%.3f");
		changed |= ImGui::SliderFloat("主砲 最大仰角##boss", &tuning.mainTurretMaxPitch, 0.0f, 1.20f, "%.2f rad");
		ImGui::TextDisabled("艦首の左右2砲台: 横回転なし、ピッチ（上下回転）だけで照準します。中央砲・主砲は従来どおり横回転します。");
	}
	if (ImGui::CollapsingHeader("後部スラスター##boss", ImGuiTreeNodeFlags_DefaultOpen)) {
		changed |= ImGui::SliderInt("スラスター数##boss", &tuning.rearThrusterCount, 0, 12);
		changed |= ImGui::DragFloat3("スラスター位置補正##boss", &tuning.rearThrusterOffset.x, 0.02f, -2.0f, 2.0f, "%.2f");
		changed |= ImGui::SliderFloat("スラスター間隔調整##boss", &tuning.rearThrusterSpacing, 0.05f, 1.20f, "%.2f");
		changed |= ImGui::SliderFloat("炎の太さ##boss", &tuning.rearThrusterRadius, 0.02f, 1.20f, "%.2f");
		changed |= ImGui::SliderFloat("炎の長さ##boss", &tuning.rearThrusterLength, 0.05f, 6.00f, "%.2f");
		changed |= ImGui::SliderFloat("炎の明るさ##boss", &tuning.rearThrusterBrightness, 0.0f, 2.50f, "%.2f");
		changed |= ImGui::SliderFloat("根元フレア半径##boss", &tuning.rearThrusterFlareRadius, 0.02f, 1.50f, "%.2f");
		ImGui::TextDisabled("中央・左・右パネルの各4穴へ、合計12基を配置できます。");
	}
	if (ImGui::CollapsingHeader("側面シールド##boss", ImGuiTreeNodeFlags_DefaultOpen)) {
		changed |= ImGui::SliderFloat("シールド不透明度##boss", &tuning.sideShieldOpacity, 0.05f, 1.0f, "%.2f");
		ImGui::TextDisabled("値を上げるほど六角形シールドが濃く見えます。保存すると次回のボス戦にも反映されます。");
	}
	if (changed) {
		if (activeBoss) {
			activeBoss->SetTuning(tuning);
			activeBoss->UpdateModel();
			setStatus("出現中のボスへ設定を反映しました。");
		} else {
			setStatus("設定しました。次のボス戦開始時に反映されます。");
		}
	}
	ImGui::Separator();
	if (ImGui::Button("ボスの位置・設定を保存##boss")) {
		SaveSettings("resources/boss_thruster_settings.json", context);
	}
	ImGui::SameLine();
	if (ImGui::Button("保存したボスの位置・設定を読み込む##boss")) {
		LoadSettings("resources/boss_thruster_settings.json", true, context);
	}
	ImGui::TextDisabled("保存先: resources/boss_thruster_settings.json (ボスHP・出現座標・砲台・スラスター・シールド設定)");
	if (context.statusMessage && !context.statusMessage->empty()) {
		ImGui::TextWrapped("%s", context.statusMessage->c_str());
	}
#else
	(void)context;
#endif
}
