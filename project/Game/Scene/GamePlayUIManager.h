#pragma once

#include <string>
#include <vector>
#include "Game/enemy/Boss.h"

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
	// 編集停止中でも撃破エフェクトの更新を許可するためのフラグ。
	bool IsDestructionEffectPreviewActive() const { return destructionEffectPreviewFrames_ > 0; }

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
	// 「敵 & イベント」内のボス戦テスト・調整に使う保持値。
	float bossBattlePosition_[3] = { 0.0f, 8.0f, 30.0f };
	Boss::Tuning bossTuning_{};
	std::string bossTuningMessage_;
	// -1: 未選択、0: ボス本体、1以降: 後部スラスター。
	int selectedBossEditorPart_ = -1;
	bool bossEditorSelectionDrag_ = false;

	// 撃破パーティクル調整用。選択中の対象とプレビュー状態を画面遷移中も保持する。
	int destructionEffectTargetIndex_ = 0;
	bool destructionEffectPreviewLoop_ = false;
	bool destructionEffectClearBeforePreview_ = true;
	bool destructionEffectAutoPreview_ = true;
	bool destructionEffectPreviewShowShockwave_ = false;
	float destructionEffectPreviewTimer_ = 0.0f;
	float destructionEffectPreviewLoopInterval_ = 1.25f;
	float destructionEffectPreviewElapsed_ = 0.0f;
	float destructionEffectPreviewDuration_ = 1.0f;
	float destructionEffectPreviewDistance_ = 12.0f;
	int destructionEffectPreviewFrames_ = 0;
};
