#pragma once

#include "3D/Object3d.h"
#include "WarpExitTimelineState.h"
#include "engine/math/MyMath.h"

#include <memory>

class Camera;
class ParticleManager;
class Player;

// ステージ開始時のワープ出口演出（空間配置・時間進行・機体演出・カメラ・ゲート描画）を一括管理する。
class WarpExitPresentationState {
public:
	void Reset();
	void Start(Player& player);
	void InitializeGateObjects();

	void UpdatePlayer(Player& player, ParticleManager* particles);
	void UpdateCamera(Camera* renderCamera, const Player* player) const;
	void UpdateGate(Camera* renderCamera);
	void DrawGate() const;

	bool IsActive() const { return timeline_.IsActive(); }
	bool IsGateVisible() const { return timeline_.IsGateVisible(); }
	int GetFrame() const { return timeline_.GetFrame(); }
	float GetYaw() const { return yaw_; }
	bool ShouldDrawMissionStartOverlay() const;
	float GetMissionStartOverlayProgress() const;

private:
	WarpExitTimelineState timeline_;
	Vector3 gatePosition_ = { 0.0f, 0.0f, 0.0f };
	Vector3 startPosition_ = { 0.0f, 0.0f, 0.0f };
	Vector3 endPosition_ = { 0.0f, 0.0f, 0.0f };
	Vector3 forward_ = { 0.0f, 0.0f, 1.0f };
	Vector3 right_ = { 1.0f, 0.0f, 0.0f };
	Vector3 cinematicCameraPosition_ = { 0.0f, 0.0f, 0.0f };
	float yaw_ = 0.0f;

	// ロード後、ステージ空間に残るワープ出口。自機はこの輪を通って出現する。
	std::unique_ptr<Object3d> ringObject_;
	// 外周だけでなく、出口の内部も発光面で満たして空間の穴に見せる。
	std::unique_ptr<Object3d> fillObject_;
	std::unique_ptr<Object3d> coreObject_;
	// 中心部を逆方向に回す二層の雲。外周だけの回転に見えない出口を作る。
	std::unique_ptr<Object3d> spiralOuterObject_;
	std::unique_ptr<Object3d> spiralInnerObject_;
};
