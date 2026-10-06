#pragma once

#include <string>
#include <vector>

class GamePlayScene;

// ゲームプレイUIおよびシミュレーションツールのUI描画を管理するクラス
class GamePlayUIManager {
public:
	friend class GamePlayScene;
	friend class SimulationManager;
	friend class MissilePresetManager;
	GamePlayUIManager(GamePlayScene* scene);
	~GamePlayUIManager() = default;

	void Initialize();
	void UpdateUI();

	void DrawGameplayActionControls();
	void DrawAmmoSettingsUI();

private:
	void DrawEditorToolbar();

	GamePlayScene* scene_ = nullptr;

	bool showSimulationWindow_ = false;
	int currentSimulationTarget_ = 0;
	int currentEngineSettingsTarget_ = 0;
	std::string simulationSaveMessage_;
	char blenderEnemyName_[64] = "Enemy";
	int blenderEnemyTypeIndex_ = 0;
	float blenderEnemyPosition_[3] = { 0.0f, 3.0f, 50.0f };
	float blenderEnemyRotationDegrees_[3] = { 0.0f, 0.0f, 0.0f };
	bool enemyMousePlacementEnabled_ = false;
	float enemyMousePlacementHeight_ = 3.0f;
	char simulationActionName_[64] = "Action1";
	std::vector<std::string> simulationActionNames_;
	int selectedSimulationActionIndex_ = 0;
	std::string simulationActionMessage_;
	int simulationPlaybackMode_ = 0;
	char missilePresetName_[64] = "MissilePreset1";
	int missilePresetTypeIndex_ = 0;
	std::vector<std::string> missilePresetNames_[2];
	int selectedMissilePresetIndex_[2] = { 0, 0 };
	std::string missilePresetMessage_;
	std::string ammoSettingsMessage_;
	std::string thrusterSettingsMessage_;
	bool showThrusterPlacementMarkers_ = false;
};
