#pragma once

#include "2D/Sprite.h"

#include <array>
#include <memory>

class Camera;
class Enemy;
class LockOnManager;
class MissileManager;
class MultiLockChargeState;
class Player;

// ロックオン枠・照準カーソル・マルチロック／飛行中ミサイルマーカーを描画する。
class ReticleOverlayRenderer {
public:
	static constexpr size_t kMaxMultiLockMarkers = 12;

	void Initialize();

	void DrawTitleBackgroundLockOn(const Enemy* lockedEnemy, const Camera* activeCamera) const;

	void DrawLockAndAimCursor(
		const Player* player,
		Enemy* lockedEnemy,
		Enemy* aimAssistEnemy,
		LockOnManager* lockOnManager,
		Camera* activeCamera,
		bool isJammed) const;

	void DrawMultiLockMarkers(
		const MultiLockChargeState& multiLockChargeState,
		const MissileManager* missileManager,
		const Camera* activeCamera,
		bool isJammed);

private:
	void DrawLockOnSprite(
		const Enemy* target,
		const Matrix4x4& viewProjectionMatrix,
		bool isJammed,
		bool isFighterLockDanger,
		const Vector3* targetPoint = nullptr,
		float targetRadius = 0.0f) const;

	void DrawMissileLockOnMarker(
		const Enemy* target,
		const Matrix4x4& viewProjectionMatrix,
		Sprite* markerSprite,
		bool isJammed,
		int lockIndex,
		int totalLockCount,
		const Vector3* targetPoint = nullptr);

	void DrawCenterAimCursor(bool isJammed, float opacity = 220.0f / 255.0f, float size = 44.0f) const;
	void DrawMouseAimCursor(bool isJammed) const;
	void DrawTargetTrackingCursor(
		const Enemy* target,
		const Matrix4x4& viewProjectionMatrix,
		bool isJammed,
		const Vector3* targetPoint = nullptr) const;

	std::unique_ptr<Sprite> aimCursorSprite_;
	std::unique_ptr<Sprite> lockOnReticleSprite_;
	std::unique_ptr<Sprite> missileLockOnReticleSprite_;
	std::array<std::unique_ptr<Sprite>, kMaxMultiLockMarkers> multiLockMarkerSprites_;
	float markerOrbitAngle_ = 0.0f;
};
