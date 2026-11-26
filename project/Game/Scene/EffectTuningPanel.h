#pragma once

#include <string>

class BoundaryShieldSettingsManager;
class Camera;
class ExplosionManager;
class Player;

// 撃破パーティクルと境界シールドのエフェクト調整UIを担う。
class EffectTuningPanel {
public:
	void UpdatePreviewTimer(float deltaTime);
	bool IsDestructionEffectPreviewActive() const { return destructionEffectPreviewFrames_ > 0; }

	void DrawDestructionEffectTab(
		ExplosionManager* explosionManager,
		const Camera* camera,
		const Player* player);
	void DrawBoundaryShieldTab(
		BoundaryShieldSettingsManager& boundaryShieldSettings,
		Player* player);

private:
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

	std::string boundaryShieldSettingsMessage_;
};
