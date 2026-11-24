#pragma once

#include "GameplayBalanceSettingsManager.h"

#include <functional>
#include <string>

class Player;

// ゲームバランス調整タブの描画に必要なデータ。
struct PlayerBalancePanelContext {
	GameplayBalanceSettingsManager* balanceManager = nullptr;
	Player* player = nullptr;
	GameplayBalanceMissileSettings missileSettings{};
	std::function<void(const GameplayBalanceMissileSettings&)> applyMissileSettings;
	std::function<void()> applyAllSettings;
	std::function<bool()> saveSettings;
	std::function<bool()> loadSettings;
	std::function<void()> drawAmmoSettings;
};

// ImGui「ゲームバランス」「スラスター位置」のプレイヤー調整パネルを描画する。
class PlayerTuningPanel {
public:
	void DrawGameplayBalanceTab(const PlayerBalancePanelContext& context);
	void DrawThrusterTuningTab(Player* player);

private:
	std::string gameplayBalanceMessage_;
	std::string thrusterSettingsMessage_;
	bool showThrusterPlacementMarkers_ = false;
};
