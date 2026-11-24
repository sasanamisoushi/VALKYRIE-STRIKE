#pragma once

class AmmoManager;
class Camera;
class CombatAlertState;
class Player;
class SongState;
class SpecialAttackState;

// 戦闘HUDのImGuiオーバーレイ描画に必要な読み取り専用データ。
struct CombatHudOverlayContext {
	float screenWidth = 0.0f;
	float screenHeight = 0.0f;
	float statusPanelX = 0.0f;
	float statusPanelY = 0.0f;
	float statusPanelWidth = 0.0f;
	float statusPanelHeight = 0.0f;
	float ammoPanelX = 0.0f;
	float normalAmmoPanelY = 0.0f;
	float homingAmmoPanelY = 0.0f;
	float normalAmmoTextY = 0.0f;
	float homingAmmoTextY = 0.0f;
	float gaugeX = 0.0f;
	float songGaugeY = 0.0f;
	float songGaugeWidth = 0.0f;
	bool isTutorialEnabled = false;
	bool isPaused = false;
	bool isGameOverActive = false;
	int gameplayElapsedFrames = 0;
	const Player* player = nullptr;
	const Camera* activeCamera = nullptr;
	const AmmoManager* ammoManager = nullptr;
	const SongState* songState = nullptr;
	const SpecialAttackState* specialAttackState = nullptr;
	const CombatAlertState* combatAlertState = nullptr;
};

// 戦闘中のImGuiオーバーレイ（境界警告・被弾方向・武器カード・リロード警告・
// ステータス数値・歌オーバードライブ演出・操作ガイド）を描画する。
class CombatHudOverlayRenderer {
public:
	void Draw(const CombatHudOverlayContext& context) const;
};
