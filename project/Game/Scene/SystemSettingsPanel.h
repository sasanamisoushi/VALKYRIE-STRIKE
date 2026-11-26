#pragma once

#include <functional>
#include <string>
#include <vector>

class AmmoManager;
class Camera;
class EnemyEventManager;
class FlyCamera;
class TutorialRequirementsManager;
struct EnemySpawnData;

struct CameraDebugPanelContext {
	bool isDebugCameraActive = false;
	FlyCamera* debugFlyCamera = nullptr;
	Camera* followCamera = nullptr;
	bool isCinematicLockOnEnabled = false;
	bool hasLockedEnemy = false;
	std::function<void(bool)> setDebugCameraActive;
	std::function<void()> resetDebugCameraToPlayer;
	std::function<void(bool)> setCinematicLockOnEnabled;
};

// カメラデバッグ・操作キー設定・チュートリアル設定・増援イベント・弾薬設定のUIを担う。
class SystemSettingsPanel {
public:
	void DrawCameraDebugTab(const CameraDebugPanelContext& context) const;
	void DrawPlayerInputBindingsTab() const;
	void DrawTutorialRequirementsTab(TutorialRequirementsManager& requirementsManager);
	void DrawEnemyEventToolSection(
		EnemyEventManager& enemyEventManager,
		std::vector<EnemySpawnData>& enemySpawns) const;
	void DrawAmmoSettingsSection(AmmoManager* ammoManager);

private:
	std::string tutorialRequirementsMessage_;
	std::string ammoSettingsMessage_;
};
