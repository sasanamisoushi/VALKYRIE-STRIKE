#include "ReticleOverlayRenderer.h"

#include "LockOnManager.h"
#include "MultiLockChargeState.h"
#include "2D/SpriteCommon.h"
#include "Game/Player/Player.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Boss.h"
#include "Game/enemy/Enemy.h"
#include "engine/Camera/Camera.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Input/Input.h"
#include "engine/Resource/TextureManager.h"
#include "engine/base/WinApp.h"
#include "engine/math/MyMath.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
constexpr char kLockOnReticleTexturePath[] = "resources/lock_on_reticle.png";
constexpr char kMissileLockOnReticleTexturePath[] = "resources/missile_lock_on_reticle.png";
constexpr char kAimCursorTexturePath[] = "resources/aim_cursor.png";
constexpr char kMultiLockMarkerTexturePath[] = "resources/multi_lock_marker.png";

Vector3 SubtractVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

float LengthSqVector3(const Vector3& v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

bool GetReticleOverlayBounds(float& minX, float& minY, float& maxX, float& maxY) {
	if (FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY) && maxX > minX && maxY > minY) {
		return true;
	}
	minX = 0.0f;
	minY = 0.0f;
	maxX = static_cast<float>(WinApp::GetClientWidth());
	maxY = static_cast<float>(WinApp::GetClientHeight());
	return maxX > minX && maxY > minY;
}
} // namespace

void ReticleOverlayRenderer::Initialize() {
	markerOrbitAngle_ = 0.0f;
	TextureManager::GetInstance()->LoadTexture(kLockOnReticleTexturePath);
	TextureManager::GetInstance()->LoadTexture(kMissileLockOnReticleTexturePath);
	TextureManager::GetInstance()->LoadTexture(kAimCursorTexturePath);
	TextureManager::GetInstance()->LoadTexture(kMultiLockMarkerTexturePath);

	SpriteCommon* spriteCommon = SpriteCommon::GetInstance();
	aimCursorSprite_ = std::make_unique<Sprite>();
	aimCursorSprite_->Initialize(spriteCommon, kAimCursorTexturePath);
	aimCursorSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	lockOnReticleSprite_ = std::make_unique<Sprite>();
	lockOnReticleSprite_->Initialize(spriteCommon, kLockOnReticleTexturePath);
	lockOnReticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	missileLockOnReticleSprite_ = std::make_unique<Sprite>();
	missileLockOnReticleSprite_->Initialize(spriteCommon, kMissileLockOnReticleTexturePath);
	missileLockOnReticleSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	for (auto& markerSprite : multiLockMarkerSprites_) {
		markerSprite = std::make_unique<Sprite>();
		markerSprite->Initialize(spriteCommon, kMultiLockMarkerTexturePath);
		markerSprite->SetAnchorPoint({ 0.5f, 0.5f });
	}
}

void ReticleOverlayRenderer::DrawLockOnSprite(
	const Enemy* target,
	const Matrix4x4& viewProjectionMatrix,
	bool isJammed,
	bool isFighterLockDanger,
	const Vector3* targetPoint,
	float targetRadius) const {
	if (!target || target->IsDead() || !lockOnReticleSprite_) {
		return;
	}

	Vector3 worldPosition = targetPoint ? *targetPoint : target->GetAimPosition();
	const float collisionRadius = targetRadius > 0.0f ? targetRadius : target->GetCollisionRadius();
	if (!targetPoint && !target->IsBoss()) {
		worldPosition.y += collisionRadius * 0.3f;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetReticleOverlayBounds(minX, minY, maxX, maxY)) {
		return;
	}

	const float width = maxX - minX;
	const float height = maxY - minY;
	const Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
	if (screenPosition.z < 0.0f || screenPosition.z > 1.0f ||
		screenPosition.x < 0.0f || screenPosition.x > width ||
		screenPosition.y < 0.0f || screenPosition.y > height) {
		return;
	}

	const float winAppWidth = static_cast<float>(WinApp::GetClientWidth());
	const float winAppHeight = static_cast<float>(WinApp::GetClientHeight());
	float spriteX = screenPosition.x * winAppWidth / width;
	float spriteY = screenPosition.y * winAppHeight / height;
	if (isJammed) {
		spriteX += (std::rand() % 11 - 5) * 1.5f;
		spriteY += (std::rand() % 11 - 5) * 1.5f;
	}

	const float reticleSize = std::clamp(76.0f + collisionRadius * 12.0f, 76.0f, 116.0f);
	const float aspectScaleX = (height / width) * (winAppWidth / winAppHeight);
	lockOnReticleSprite_->SetPosition({ spriteX, spriteY });
	lockOnReticleSprite_->SetSize({ reticleSize * aspectScaleX, reticleSize });
	lockOnReticleSprite_->SetColor(isFighterLockDanger
		? Vector4{ 1.0f, 0.18f, 0.12f, 1.0f }
		: Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
	lockOnReticleSprite_->Update();
	lockOnReticleSprite_->Draw();
}

void ReticleOverlayRenderer::DrawMissileLockOnMarker(
	const Enemy* target,
	const Matrix4x4& viewProjectionMatrix,
	Sprite* markerSprite,
	bool isJammed,
	int lockIndex,
	int totalLockCount,
	const Vector3* targetPoint) {
	if (!target || target->IsDead() || !markerSprite) {
		return;
	}

	Vector3 worldPosition = targetPoint ? *targetPoint : target->GetAimPosition();
	const float collisionRadius = target->GetCollisionRadius();
	if (!targetPoint && !target->IsBoss()) {
		worldPosition.y += collisionRadius * 0.3f;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetReticleOverlayBounds(minX, minY, maxX, maxY)) {
		return;
	}

	const float width = maxX - minX;
	const float height = maxY - minY;
	Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
	if (screenPosition.z < 0.0f) {
		return;
	}
	screenPosition.x = std::clamp(screenPosition.x, 20.0f, width - 20.0f);
	screenPosition.y = std::clamp(screenPosition.y, 20.0f, height - 20.0f);

	const float winAppWidth = static_cast<float>(WinApp::GetClientWidth());
	const float winAppHeight = static_cast<float>(WinApp::GetClientHeight());
	float spriteX = screenPosition.x * winAppWidth / width;
	float spriteY = screenPosition.y * winAppHeight / height;
	if (isJammed) {
		spriteX += (std::rand() % 11 - 5) * 1.5f;
		spriteY += (std::rand() % 11 - 5) * 1.5f;
	}

	const float baseReticleSize = std::clamp(32.0f + collisionRadius * 4.0f, 28.0f, 56.0f);
	const float aspectScaleX = (height / width) * (winAppWidth / winAppHeight);

	markerOrbitAngle_ += 0.025f;
	const int effectiveTotal = (std::max)(totalLockCount, 1);
	const float offsetDistance = (totalLockCount > 1) ? std::clamp(28.0f + collisionRadius * 6.0f, 24.0f, 48.0f) : 0.0f;
	const float angle = (3.14159265f * 2.0f / static_cast<float>(effectiveTotal)) * static_cast<float>(lockIndex - 1) + markerOrbitAngle_;

	const float finalX = spriteX + std::cos(angle) * offsetDistance;
	const float finalY = spriteY + std::sin(angle) * offsetDistance;
	markerSprite->SetColor({ 1.0f, 0.15f, 0.15f, 1.0f });
	markerSprite->SetPosition({ finalX, finalY });
	markerSprite->SetSize({ baseReticleSize * aspectScaleX, baseReticleSize });
	markerSprite->Update();
	markerSprite->Draw();
}

void ReticleOverlayRenderer::DrawCenterAimCursor(bool isJammed, float opacity, float size) const {
	if (!aimCursorSprite_) {
		return;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetReticleOverlayBounds(minX, minY, maxX, maxY)) {
		return;
	}

	const float winAppWidth = static_cast<float>(WinApp::GetClientWidth());
	const float winAppHeight = static_cast<float>(WinApp::GetClientHeight());
	const float width = maxX - minX;
	const float height = maxY - minY;
	float spriteX = winAppWidth * 0.5f;
	float spriteY = winAppHeight * 0.5f;
	if (isJammed) {
		spriteX += (std::rand() % 11 - 5) * 1.5f;
		spriteY += (std::rand() % 11 - 5) * 1.5f;
	}

	const float aspectScaleX = (height / width) * (winAppWidth / winAppHeight);
	aimCursorSprite_->SetPosition({ spriteX, spriteY });
	aimCursorSprite_->SetSize({ size * aspectScaleX, size });
	aimCursorSprite_->SetColor({ 1.0f, 1.0f, 1.0f, opacity });
	aimCursorSprite_->Update();
	aimCursorSprite_->Draw();
}

void ReticleOverlayRenderer::DrawMouseAimCursor(bool isJammed) const {
	if (!aimCursorSprite_) {
		return;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetReticleOverlayBounds(minX, minY, maxX, maxY)) {
		return;
	}

	const float width = maxX - minX;
	const float height = maxY - minY;
	POINT mousePosition{};
	if (!Input::GetInstance()->GetMouseClientPosition(mousePosition)) {
		DrawCenterAimCursor(isJammed, 0.32f, 30.0f);
		return;
	}

	float spriteX = static_cast<float>(mousePosition.x);
	float spriteY = static_cast<float>(mousePosition.y);
	if (isJammed) {
		spriteX += (std::rand() % 11 - 5) * 1.5f;
		spriteY += (std::rand() % 11 - 5) * 1.5f;
	}

	const float aspectScaleX = (height / width) *
		(static_cast<float>(WinApp::GetClientWidth()) / static_cast<float>(WinApp::GetClientHeight()));
	aimCursorSprite_->SetPosition({ spriteX, spriteY });
	aimCursorSprite_->SetSize({ 36.0f * aspectScaleX, 36.0f });
	aimCursorSprite_->SetColor({ 0.35f, 0.92f, 1.0f, 0.90f });
	aimCursorSprite_->Update();
	aimCursorSprite_->Draw();
}

void ReticleOverlayRenderer::DrawTargetTrackingCursor(
	const Enemy* target,
	const Matrix4x4& viewProjectionMatrix,
	bool isJammed,
	const Vector3* targetPoint) const {
	if (!target || !aimCursorSprite_) {
		return;
	}

	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	if (!GetReticleOverlayBounds(minX, minY, maxX, maxY)) {
		return;
	}

	const float width = maxX - minX;
	const float height = maxY - minY;
	const Vector3 worldPosition = targetPoint ? *targetPoint : target->GetAimPosition();
	Vector3 screenPosition = MyMath::WorldToScreen(worldPosition, viewProjectionMatrix, width, height);
	if (screenPosition.z < 0.0f) {
		return;
	}

	screenPosition.x = std::clamp(screenPosition.x, 22.0f, width - 22.0f);
	screenPosition.y = std::clamp(screenPosition.y, 22.0f, height - 22.0f);
	float spriteX = screenPosition.x * static_cast<float>(WinApp::GetClientWidth()) / width;
	float spriteY = screenPosition.y * static_cast<float>(WinApp::GetClientHeight()) / height;
	if (isJammed) {
		spriteX += (std::rand() % 11 - 5) * 1.5f;
		spriteY += (std::rand() % 11 - 5) * 1.5f;
	}

	const float aspectScaleX = (height / width) *
		(static_cast<float>(WinApp::GetClientWidth()) / static_cast<float>(WinApp::GetClientHeight()));
	aimCursorSprite_->SetPosition({ spriteX, spriteY });
	aimCursorSprite_->SetSize({ 32.0f * aspectScaleX, 32.0f });
	aimCursorSprite_->SetColor({ 0.35f, 0.92f, 1.0f, 0.90f });
	aimCursorSprite_->Update();
	aimCursorSprite_->Draw();
}

void ReticleOverlayRenderer::DrawTitleBackgroundLockOn(const Enemy* lockedEnemy, const Camera* activeCamera) const {
	if (lockedEnemy && !lockedEnemy->IsDead() && activeCamera) {
		DrawLockOnSprite(lockedEnemy, activeCamera->GetViewProjectionMatrix(), false, false);
	}
}

void ReticleOverlayRenderer::DrawLockAndAimCursor(
	const Player* player,
	Enemy* lockedEnemy,
	Enemy* aimAssistEnemy,
	LockOnManager* lockOnManager,
	Camera* activeCamera,
	bool isJammed) const {
	if (!activeCamera) {
		return;
	}

	const bool isGerwalk = player && player->GetCurrentMode() == PlayerMode::Gerwalk;
	Enemy* overlayTarget = lockedEnemy ? lockedEnemy : aimAssistEnemy;
	Vector3 lockedTargetPosition{};
	const Vector3* lockedTargetPoint = nullptr;
	float lockedTargetRadius = 0.0f;

	// 敵のロックオン枠は両形態で表示する。
	if (overlayTarget && !overlayTarget->IsDead()) {
		const bool isFighterLockDanger = lockedEnemy && lockOnManager && lockOnManager->IsFighterLockDanger();
		if (overlayTarget == lockedEnemy && lockOnManager) {
			if (lockOnManager->HasLockedBossTurret() || overlayTarget->IsBoss()) {
				lockedTargetPosition = lockOnManager->GetLockedTargetPosition();
				lockedTargetPoint = &lockedTargetPosition;
				lockedTargetRadius = lockOnManager->GetLockedTargetRadius();
			}
		}
		DrawLockOnSprite(overlayTarget, activeCamera->GetViewProjectionMatrix(), isJammed,
			isFighterLockDanger, lockedTargetPoint, lockedTargetRadius);
	}

	if (isGerwalk && lockedEnemy && !lockedEnemy->IsDead()) {
		// ガウォークでは、ロックオン成立後にゲーム内カーソルも対象位置へ固定する。
		DrawTargetTrackingCursor(lockedEnemy, activeCamera->GetViewProjectionMatrix(), isJammed, lockedTargetPoint);
	} else if (isGerwalk) {
		// 非ロック時は、マウス位置をそのまま狙いのターゲットカーソルとして使う。
		DrawMouseAimCursor(isJammed);
	} else {
		// ファイターは機体正面の照準として中央固定レティクルを常に表示する。
		DrawCenterAimCursor(isJammed);
	}
}

void ReticleOverlayRenderer::DrawMultiLockMarkers(
	const MultiLockChargeState& multiLockChargeState,
	const MissileManager* missileManager,
	const Camera* activeCamera,
	bool isJammed) {
	if (!activeCamera) {
		return;
	}

	// 赤色のターゲットマーカー：マルチロックチャージ中または飛行中ミサイルが存在する時のみ表示
	if (multiLockChargeState.IsCharging()) {
		std::vector<MultiLockTarget> validTargets;
		for (const auto& target : multiLockChargeState.GetTargets()) {
			if (target.enemy && !target.enemy->IsDead()) {
				validTargets.push_back(target);
			}
		}
		if (!validTargets.empty()) {
			const int targetCount = static_cast<int>(validTargets.size());
			const int displayCount = (std::min)(targetCount, static_cast<int>(multiLockMarkerSprites_.size()));
			for (int i = 0; i < displayCount; ++i) {
				const auto& target = validTargets[static_cast<size_t>(i)];
				const Vector3* pointPtr = target.hasTargetPoint ? &target.targetPoint : nullptr;
				DrawMissileLockOnMarker(target.enemy, activeCamera->GetViewProjectionMatrix(),
					multiLockMarkerSprites_[static_cast<size_t>(i)].get(), isJammed, i + 1, displayCount, pointPtr);
			}
		}
	} else if (missileManager) {
		struct ActiveMarker {
			Enemy* target = nullptr;
			bool hasPoint = false;
			Vector3 targetPoint{};
		};
		std::vector<ActiveMarker> activeMarkers;
		for (const auto& missile : missileManager->GetMissiles()) {
			if (missile && !missile->IsDead()) {
				Enemy* t = missile->GetTarget();
				if (t && !t->IsDead()) {
					const bool hasPoint = missile->HasTargetPoint();
					const Vector3 pt = missile->GetTargetPoint();
					bool exists = false;
					for (const auto& m : activeMarkers) {
						if (m.target == t) {
							if (hasPoint && m.hasPoint) {
								if (LengthSqVector3(SubtractVector3(m.targetPoint, pt)) < 1.0f) {
									exists = true;
									break;
								}
							} else if (!hasPoint && !m.hasPoint) {
								exists = true;
								break;
							}
						}
					}
					if (!exists) {
						activeMarkers.push_back({ t, hasPoint, pt });
					}
				}
			}
		}
		if (!activeMarkers.empty()) {
			const int totalCount = (std::min)(static_cast<int>(activeMarkers.size()), static_cast<int>(multiLockMarkerSprites_.size()));
			for (int i = 0; i < totalCount; ++i) {
				const auto& marker = activeMarkers[static_cast<size_t>(i)];
				DrawMissileLockOnMarker(marker.target, activeCamera->GetViewProjectionMatrix(),
					multiLockMarkerSprites_[static_cast<size_t>(i)].get(), isJammed, i + 1, totalCount,
					marker.hasPoint ? &marker.targetPoint : nullptr);
			}
		}
	}
}
