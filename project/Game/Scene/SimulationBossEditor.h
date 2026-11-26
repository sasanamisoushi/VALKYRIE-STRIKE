#pragma once

#include "Game/enemy/Boss.h"

#include <functional>
#include <list>
#include <memory>
#include <string>

class BossEncounterState;
class Enemy;
class GameOverSequence;
class Obstacle;

struct SimulationBossEditorContext {
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	const std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	BossEncounterState* bossEncounterState = nullptr;
	GameOverSequence* gameOverSequence = nullptr;
	bool* isEditorPreviewPlaying = nullptr;
	float* bossBattlePosition = nullptr; // float[3]
	Boss::Tuning* bossTuning = nullptr;
	std::string* statusMessage = nullptr;
	std::function<void()> clearAllEnemyReferences;
};

// ボス戦テスト開始、砲台・スラスター・シールド調整UI、および設定JSONの保存・読込を担う。
class SimulationBossEditor {
public:
	static bool SaveSettings(const std::string& filePath, const SimulationBossEditorContext& context);
	static bool LoadSettings(const std::string& filePath, bool verbose, const SimulationBossEditorContext& context);
	static void DrawControls(const SimulationBossEditorContext& context);
};
