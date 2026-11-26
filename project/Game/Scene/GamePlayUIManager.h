#pragma once

#include <string>
#include <vector>
#include "BossEditorPanel.h"
#include "EffectTuningPanel.h"
#include "PlayerTuningPanel.h"
#include "SimulationControlPanel.h"
#include "StageEditorPanel.h"
#include "SystemSettingsPanel.h"
#include "Game/enemy/Boss.h"

class GamePlayScene;

// ゲームプレイUIおよびシミュレーションツールのUI描画を管理するクラス
class GamePlayUIManager {
public:
	friend class SimulationManager;
	GamePlayUIManager(GamePlayScene* scene);
	~GamePlayUIManager() = default;

	void Initialize();
	void UpdateUI();
	// 編集停止中でも撃破エフェクトの更新を許可するためのフラグ。
	bool IsDestructionEffectPreviewActive() const { return effectTuningPanel_.IsDestructionEffectPreviewActive(); }

	void DrawGameplayActionControls();
	void DrawAmmoSettingsUI();

	bool IsEnemyMousePlacementEnabled() const { return enemyMousePlacementEnabled_; }
	float GetEnemyMousePlacementHeight() const { return enemyMousePlacementHeight_; }
	int GetBlenderEnemyTypeIndex() const { return blenderEnemyTypeIndex_; }
	const char* GetBlenderEnemyName() const { return blenderEnemyName_; }
	const float* GetBlenderEnemyRotationDegrees() const { return blenderEnemyRotationDegrees_; }
	void SetSimulationSaveMessage(const std::string& message) { simulationSaveMessage_ = message; }
	bool IsBossEditorTargetSelected() const { return currentEngineSettingsTarget_ == 8; }
	int GetSelectedBossEditorPart() const { return selectedBossEditorPart_; }
	void SetSelectedBossEditorPart(int partIndex) { selectedBossEditorPart_ = partIndex; }
	bool IsBossEditorSelectionDragging() const { return bossEditorSelectionDrag_; }
	void SetBossEditorSelectionDragging(bool dragging) { bossEditorSelectionDrag_ = dragging; }
	void SetBossBattlePosition(const Vector3& pos) {
		bossBattlePosition_[0] = pos.x;
		bossBattlePosition_[1] = pos.y;
		bossBattlePosition_[2] = pos.z;
	}
	void SetBossTuning(const Boss::Tuning& tuning) { bossTuning_ = tuning; }
	Vector3 GetBossBattlePosition() const { return { bossBattlePosition_[0], bossBattlePosition_[1], bossBattlePosition_[2] }; }
	const Boss::Tuning& GetBossTuning() const { return bossTuning_; }
	int GetCurrentSimulationTarget() const { return currentSimulationTarget_; }

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
	PlayerTuningPanel playerTuningPanel_;
	SimulationControlPanel simulationControlPanel_;
	StageEditorPanel stageEditorPanel_;
	EffectTuningPanel effectTuningPanel_;
	SystemSettingsPanel systemSettingsPanel_;
	// 「敵 & イベント」内のボス戦テスト・調整に使う保持値。
	float bossBattlePosition_[3] = { 0.0f, 8.0f, 30.0f };
	Boss::Tuning bossTuning_{};
	std::string bossTuningMessage_;
	BossEditorPanel bossEditorPanel_;
	// -1: 未選択、0: ボス本体、1以降: 後部スラスター。
	int selectedBossEditorPart_ = -1;
	bool bossEditorSelectionDrag_ = false;
};
