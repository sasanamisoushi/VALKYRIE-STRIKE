#include "BossEditorPanel.h"

#include "SimulationManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyEventManager.h"
#include "engine/Utility/StageLoader.h"

#ifdef ENABLE_IMGUI
#include <externals/imgui/imgui.h>
#endif

#include <string>
#include <vector>

void BossEditorPanel::Draw(const BossEditorPanelContext& context) {
#ifdef ENABLE_IMGUI
	ImGui::Text("敵イベント");
	ImGui::TextDisabled("敵撃破後に別の敵を出現させる増援イベントを作成します。");

	if (context.enemySpawns && context.enemyEventManager) {
		auto& enemySpawns = *context.enemySpawns;
		auto& enemyEventManager = *context.enemyEventManager;
		std::vector<const char*> enemyNames;
		for (const auto& spawn : enemySpawns) {
			enemyNames.push_back(spawn.name.c_str());
		}
		if (enemyNames.empty()) {
			ImGui::TextDisabled("登録済みの敵配置がありません。先に「敵・障害物」で敵を追加してください。");
		} else {
			if (selectedTrigger_ >= static_cast<int>(enemyNames.size())) selectedTrigger_ = 0;
			if (selectedTarget_ >= static_cast<int>(enemyNames.size())) selectedTarget_ = 0;
			ImGui::Combo("トリガーとなる敵##event", &selectedTrigger_, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::Combo("出現する敵##event", &selectedTarget_, enemyNames.data(), static_cast<int>(enemyNames.size()));
			ImGui::DragInt("出現までのディレイ（フレーム）##event", &eventDelay_, 1, 0, 600);
			if (ImGui::Button("イベントを追加##event")) {
				enemyEventManager.AddEvent(enemyNames[selectedTrigger_], enemyNames[selectedTarget_], eventDelay_);
				enemySpawns[selectedTarget_].isInitialSpawn = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("イベントを保存##event")) {
				enemyEventManager.SaveEvents("resources/enemy_events.json");
			}
			for (size_t index = 0; index < enemyEventManager.GetEvents().size(); ++index) {
				const auto& event = enemyEventManager.GetEvents()[index];
				ImGui::Text("[%d] %s が死んだら %d F後に %s が出現",
					static_cast<int>(index), event.triggerEnemyName.c_str(), event.delayFrames, event.targetEnemyName.c_str());
				ImGui::SameLine();
				if (ImGui::Button(("削除##event" + std::to_string(index)).c_str())) {
					enemyEventManager.RemoveEvent(index);
					break;
				}
			}
		}
	}

	ImGui::Separator();
	if (context.simulationManager) {
		context.simulationManager->DrawBossBattleControls();
	}

	ImGui::Separator();
	ImGui::Text("選択中のボス部位");
	ImGui::TextDisabled("Game Viewで本体または推進炎を左クリックし、ドラッグで移動します。");

	Boss* activeBoss = nullptr;
	if (context.enemies) {
		for (const auto& enemy : *context.enemies) {
			if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
				activeBoss = dynamic_cast<Boss*>(enemy.get());
				if (activeBoss) break;
			}
		}
	}

	if (!context.selectedBossEditorPart) {
		return;
	}
	int& selectedPart = *context.selectedBossEditorPart;

	if (!activeBoss) {
		selectedPart = -1;
		ImGui::TextDisabled("編集するボスがいません。「この位置にボス戦を開始」で配置してください。");
	} else if (selectedPart == 0) {
		Vector3 position = activeBoss->GetPosition();
		ImGui::TextColored(ImVec4(0.35f, 0.90f, 1.0f, 1.0f), "ボス本体");
		if (ImGui::DragFloat3("位置 (X, Y, Z)##selectedBossBody", &position.x, 0.05f)) {
			activeBoss->SetPosition(position);
			activeBoss->UpdateModel();
			if (context.bossBattlePosition) {
				context.bossBattlePosition[0] = position.x;
				context.bossBattlePosition[1] = position.y;
				context.bossBattlePosition[2] = position.z;
			}
		}
		if (ImGui::Button("ボスの位置を保存##saveBossBodyPosition") && context.simulationManager) {
			context.simulationManager->SaveBossThrusterSettings("resources/boss_thruster_settings.json");
		}
		ImGui::SameLine();
		if (ImGui::Button("保存した位置を読込##loadBossBodyPosition") && context.simulationManager) {
			context.simulationManager->LoadBossThrusterSettings("resources/boss_thruster_settings.json");
		}
	} else if (selectedPart > 0 &&
		static_cast<size_t>(selectedPart - 1) < activeBoss->GetRearThrusterCount()) {
		const size_t thrusterIndex = static_cast<size_t>(selectedPart - 1);
		Boss::Tuning tuning = activeBoss->GetTuning();
		Vector3& individualOffset = tuning.rearThrusterIndividualOffsets[thrusterIndex];
		const Vector3 worldPosition = activeBoss->GetRearThrusterWorldPosition(thrusterIndex);
		ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.30f, 1.0f), "後部スラスター %d", selectedPart);
		ImGui::Text("現在の位置: %.2f, %.2f, %.2f", worldPosition.x, worldPosition.y, worldPosition.z);
		if (ImGui::DragFloat3("個別位置補正 (X, Y, Z)##selectedBossThruster", &individualOffset.x, 0.02f)) {
			activeBoss->SetTuning(tuning);
			activeBoss->UpdateModel();
			if (context.bossTuning) {
				*context.bossTuning = tuning;
			}
		}
		if (ImGui::Button("このスラスターの位置補正を戻す")) {
			tuning.rearThrusterIndividualOffsets[thrusterIndex] = { 0.0f, 0.0f, 0.0f };
			activeBoss->SetTuning(tuning);
			activeBoss->UpdateModel();
			if (context.bossTuning) {
				*context.bossTuning = tuning;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("スラスター位置を保存##saveThrusterPosition") && context.simulationManager) {
			context.simulationManager->SaveBossThrusterSettings("resources/boss_thruster_settings.json");
		}
		ImGui::TextDisabled("数値はボス本体に対するローカル位置補正です。");
	} else {
		selectedPart = -1;
		ImGui::TextDisabled("Game Viewで編集する部位を選択してください。");
	}
#else
	(void)context;
#endif
}
