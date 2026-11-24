#pragma once

#include "Game/enemy/Boss.h"

#include <list>
#include <memory>
#include <vector>

class Enemy;
class EnemyEventManager;
class SimulationManager;
struct EnemySpawnData;

// ボス編集・敵イベントパネル描画に必要なデータ。
struct BossEditorPanelContext {
	std::vector<EnemySpawnData>* enemySpawns = nullptr;
	EnemyEventManager* enemyEventManager = nullptr;
	SimulationManager* simulationManager = nullptr;
	const std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	int* selectedBossEditorPart = nullptr;
	float* bossBattlePosition = nullptr;
	Boss::Tuning* bossTuning = nullptr;
};

// ImGui「敵イベント・ボス編集」パネルを描画する。
class BossEditorPanel {
public:
	void Draw(const BossEditorPanelContext& context);

private:
	int selectedTrigger_ = 0;
	int selectedTarget_ = 0;
	int eventDelay_ = 60;
};
