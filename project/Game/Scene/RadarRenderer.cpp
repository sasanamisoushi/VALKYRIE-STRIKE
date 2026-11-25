#include "RadarRenderer.h"

#include "2D/SpriteCommon.h"
#include "Game/Player/Player.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "MultiLockChargeState.h"
#include "engine/Camera/FlyCamera.h"
#include "engine/Debug/ImGuiManager.h"
#include "engine/Resource/TextureManager.h"
#include "engine/base/WinApp.h"

#ifdef ENABLE_IMGUI
#include <externals/imgui/imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
bool GetOverlayBounds(float& minX, float& minY, float& maxX, float& maxY) {
	if (ImGuiManager::IsVisible() && FlyCamera::GetGameViewBounds(minX, minY, maxX, maxY) && maxX > minX && maxY > minY) {
		return true;
	}

	minX = 0.0f;
	minY = 0.0f;
	maxX = static_cast<float>(WinApp::GetClientWidth());
	maxY = static_cast<float>(WinApp::GetClientHeight());
	return maxX > minX && maxY > minY;
}
}

void RadarRenderer::Initialize() {
	sweepAngle_ = 0.0f;

	TextureManager::GetInstance()->LoadTexture("resources/radar_frame.png");
	frameSprite_ = std::make_unique<Sprite>();
	frameSprite_->Initialize(SpriteCommon::GetInstance(), "resources/radar_frame.png");
	frameSprite_->SetAnchorPoint({ 0.5f, 0.5f });

	sweepSprite_ = std::make_unique<Sprite>();
	sweepSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
	sweepSprite_->SetAnchorPoint({ 0.0f, 0.5f });

	for (auto& blipSprite : blipSprites_) {
		blipSprite = std::make_unique<Sprite>();
		blipSprite->Initialize(SpriteCommon::GetInstance(), "resources/white1x1.png");
		blipSprite->SetAnchorPoint({ 0.5f, 0.5f });
	}
}

void RadarRenderer::DrawAltitudeSpriteElement(const Vector2& position, const Vector2& size, const Vector4& color) const {
	if (!sweepSprite_) {
		return;
	}
	sweepSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	sweepSprite_->SetRotation(0.0f);
	sweepSprite_->SetPosition(position);
	sweepSprite_->SetSize(size);
	sweepSprite_->SetColor(color);
	sweepSprite_->Update();
	sweepSprite_->Draw();
}

void RadarRenderer::Draw(
	const Player* player,
	const std::list<std::unique_ptr<Enemy>>& enemies,
	const Enemy* lockedEnemy,
	const MultiLockChargeState& multiLockChargeState,
	const MissileManager* missileManager) {
	if (!player || !frameSprite_) {
		return;
	}

	constexpr float kRadarRange = 250.0f;
	const float screenWidth = static_cast<float>(WinApp::GetClientWidth());
	const float screenHeight = static_cast<float>(WinApp::GetClientHeight());
	const float radarSize = (std::min)(230.0f, screenHeight * 0.30f);
	const float radarRadius = radarSize * 0.39f;
	const Vector2 radarCenter = {
		// 右側に高度メモリ専用の余白を確保するため、レーダー本体を少し左へ寄せる。
		screenWidth - radarSize * 0.5f - 56.0f,
		radarSize * 0.5f + 20.0f
	};

	bool drawVectorRadarMarkers = false;
#ifdef ENABLE_IMGUI
	ImDrawList* radarDrawList = nullptr;
	float radarOverlayMinX = 0.0f;
	float radarOverlayMinY = 0.0f;
	float radarOverlayWidth = screenWidth;
	float radarOverlayHeight = screenHeight;
	if (ImGui::GetCurrentContext()) {
		float maxX = 0.0f;
		float maxY = 0.0f;
		if (GetOverlayBounds(radarOverlayMinX, radarOverlayMinY, maxX, maxY)) {
			radarOverlayWidth = maxX - radarOverlayMinX;
			radarOverlayHeight = maxY - radarOverlayMinY;
			radarDrawList = ImGui::GetForegroundDrawList();
			drawVectorRadarMarkers = radarDrawList != nullptr;
		}
	}
	const auto toRadarOverlay = [&](const Vector2& position) {
		return ImVec2(radarOverlayMinX + position.x * radarOverlayWidth / screenWidth,
			radarOverlayMinY + position.y * radarOverlayHeight / screenHeight);
	};
	if (drawVectorRadarMarkers) {
		// 枠もマーカーと同じ描画先・座標変換に統一する。
		radarDrawList->PushClipRect(ImVec2(radarOverlayMinX, radarOverlayMinY),
			ImVec2(radarOverlayMinX + radarOverlayWidth, radarOverlayMinY + radarOverlayHeight), true);
		radarDrawList->AddImage((ImTextureID)TextureManager::GetInstance()->GetSrvHandleGPU("resources/radar_frame.png").ptr,
			toRadarOverlay({ radarCenter.x - radarSize * 0.5f, radarCenter.y - radarSize * 0.5f }),
			toRadarOverlay({ radarCenter.x + radarSize * 0.5f, radarCenter.y + radarSize * 0.5f }),
			ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 245));
	}
#endif
	if (!drawVectorRadarMarkers) {
		frameSprite_->SetPosition(radarCenter);
		frameSprite_->SetSize({ radarSize, radarSize });
		frameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f });
		frameSprite_->Update();
		frameSprite_->Draw();
	}

	sweepAngle_ += 0.025f;
	if (sweepAngle_ >= 6.2831853f) {
		sweepAngle_ -= 6.2831853f;
	}
#ifdef ENABLE_IMGUI
	if (drawVectorRadarMarkers) {
		radarDrawList->AddLine(toRadarOverlay(radarCenter),
			toRadarOverlay({ radarCenter.x + std::sin(sweepAngle_) * radarRadius,
				radarCenter.y - std::cos(sweepAngle_) * radarRadius }), IM_COL32(0, 230, 255, 122),
			2.0f * radarOverlayWidth / screenWidth);
	}
#endif
	if (!drawVectorRadarMarkers && sweepSprite_) {
		sweepSprite_->SetAnchorPoint({ 0.0f, 0.5f });
		sweepSprite_->SetPosition(radarCenter);
		sweepSprite_->SetSize({ radarRadius, 2.0f });
		sweepSprite_->SetRotation(sweepAngle_ - 1.5707963f);
		sweepSprite_->SetColor({ 0.0f, 0.9f, 1.0f, 0.48f });
		sweepSprite_->Update();
		sweepSprite_->Draw();
	}

	const Vector3 playerPosition = player->GetPosition();
	const Vector3 playerForward = player->GetForwardVector();
	const float forwardLength = std::sqrt(
		playerForward.x * playerForward.x + playerForward.z * playerForward.z);
	const float forwardX = forwardLength > 0.0001f ? playerForward.x / forwardLength : 0.0f;
	const float forwardZ = forwardLength > 0.0001f ? playerForward.z / forwardLength : 1.0f;
	const float rightX = forwardZ;
	const float rightZ = -forwardX;

	// レーダーの右側に、プレイヤーを基準にした相対高度の目盛りを描く。
	// 敵機の高度マーカーはレーダーの三角マーカーと同じ赤色にすることで、平面上の
	// 位置と高低差を一目で対応付けられるようにしている。
	constexpr float kAltitudeRange = 80.0f;
	// レーダー円の外側へ完全に出し、ブリップや円周線と重ならない位置に固定する。
	const float altitudeScaleX = radarCenter.x + radarSize * 0.56f;
	const float altitudeScaleHalfHeight = radarRadius * 0.88f;
	const float altitudeScaleHeight = altitudeScaleHalfHeight * 2.0f;
#ifdef ENABLE_IMGUI
	if (drawVectorRadarMarkers) {
		const ImU32 scaleColor = IM_COL32(92, 238, 255, 230);
		const ImU32 scaleDimColor = IM_COL32(52, 156, 184, 180);
		radarDrawList->AddLine(toRadarOverlay({ altitudeScaleX, radarCenter.y - altitudeScaleHalfHeight }),
			toRadarOverlay({ altitudeScaleX, radarCenter.y + altitudeScaleHalfHeight }), scaleColor, 2.0f);
		for (int tick = -4; tick <= 4; ++tick) {
			const float tickY = radarCenter.y - static_cast<float>(tick) * (altitudeScaleHalfHeight / 4.0f);
			const float tickWidth = tick == 0 ? 14.0f : ((tick % 2 == 0) ? 10.0f : 7.0f);
			radarDrawList->AddLine(toRadarOverlay({ altitudeScaleX - tickWidth * 0.5f, tickY }),
				toRadarOverlay({ altitudeScaleX + tickWidth * 0.5f, tickY }),
				tick == 0 ? scaleColor : scaleDimColor, tick == 0 ? 2.0f : 1.0f);
		}
		// 自機は中央の水色三角。敵の赤三角と混同しない。
		radarDrawList->AddTriangleFilled(
			toRadarOverlay({ altitudeScaleX - 13.0f, radarCenter.y }),
			toRadarOverlay({ altitudeScaleX - 21.0f, radarCenter.y - 4.5f }),
			toRadarOverlay({ altitudeScaleX - 21.0f, radarCenter.y + 4.5f }),
			IM_COL32(105, 240, 255, 255));
	}
#endif

	// ImGui が利用できない構成では従来のスプライト描画をフォールバックにする。
	if (!drawVectorRadarMarkers) {
		DrawAltitudeSpriteElement({ altitudeScaleX, radarCenter.y }, { 2.0f, altitudeScaleHeight },
			{ 0.12f, 0.95f, 0.72f, 0.72f });
		for (int tick = -4; tick <= 4; ++tick) {
			const float tickY = radarCenter.y - static_cast<float>(tick) * (altitudeScaleHalfHeight / 4.0f);
			const float tickWidth = tick == 0 ? 12.0f : ((tick % 2 == 0) ? 9.0f : 6.0f);
			DrawAltitudeSpriteElement({ altitudeScaleX, tickY }, { tickWidth, tick == 0 ? 2.5f : 1.5f },
				{ 0.12f, 0.95f, 0.72f, tick == 0 ? 0.95f : 0.58f });
		}
		// 中央の水色マーカーが自機の高度（相対高度 0）を示す。
		DrawAltitudeSpriteElement({ altitudeScaleX - 8.0f, radarCenter.y }, { 10.0f, 4.0f },
			{ 0.25f, 0.90f, 1.0f, 1.0f });
	}

	size_t blipIndex = 0;
	std::vector<float> placedAltitudeMarkerYs;
	placedAltitudeMarkerYs.reserve(enemies.size());
	for (const auto& enemy : enemies) {
		if (!enemy || enemy->IsDead() || blipIndex >= blipSprites_.size()) {
			continue;
		}

		const Vector3 enemyPosition = enemy->GetPosition();
		const float deltaX = enemyPosition.x - playerPosition.x;
		const float deltaZ = enemyPosition.z - playerPosition.z;
		const float localRight = deltaX * rightX + deltaZ * rightZ;
		const float localForward = deltaX * forwardX + deltaZ * forwardZ;
		const float distance = std::sqrt(localRight * localRight + localForward * localForward);
		const float positionScale = distance > kRadarRange && distance > 0.0001f
			? kRadarRange / distance
			: 1.0f;

		Sprite* blip = blipSprites_[blipIndex++].get();
		blip->SetPosition({
			radarCenter.x + (localRight * positionScale / kRadarRange) * radarRadius,
			radarCenter.y - (localForward * positionScale / kRadarRange) * radarRadius
		});
		int multiLockCount = 0;
		if (multiLockChargeState.IsCharging()) {
			multiLockCount = multiLockChargeState.CountTargetsForEnemy(enemy.get());
		} else if (missileManager) {
			// 発射後も着弾するまでマーカーを残し、どの敵に誘導弾が向かっているか分かるようにする。
			for (const auto& missile : missileManager->GetMissiles()) {
				if (missile && !missile->IsDead() && missile->GetTarget() == enemy.get()) {
					++multiLockCount;
				}
			}
		}

		const float multiLockPulse = 0.78f + 0.22f * std::sin(sweepAngle_ * 5.0f);
		const float blipSize = multiLockCount > 0 ? 12.0f + multiLockPulse * 3.0f
			: (enemy->IsBoss() ? 12.0f : (enemy.get() == lockedEnemy ? 9.0f : 6.0f));
		blip->SetSize({ blipSize, blipSize });
		Vector4 markerColor{};
		if (multiLockCount > 0) {
			markerColor = { 1.0f, 0.10f, 0.12f, multiLockPulse };
		} else if (enemy.get() == lockedEnemy) {
			markerColor = { 1.0f, 0.95f, 0.05f, 1.0f };
		} else if (enemy->IsBoss()) {
			markerColor = { 1.0f, 0.12f, 0.05f, 1.0f };
		} else {
			markerColor = { 1.0f, 0.38f, 0.05f, 1.0f };
		}
		if (drawVectorRadarMarkers) {
			// ベクター描画時は下地の四角いスプライトを描かない。
			blip->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		} else if (multiLockCount > 0) {
			// 赤い脈動点は、マルチロックされた敵（または同敵へ飛行中の誘導弾）を表す。
			blip->SetColor(markerColor);
		} else if (enemy.get() == lockedEnemy) {
			blip->SetColor({ 1.0f, 0.95f, 0.05f, 1.0f });
		} else if (enemy->IsBoss()) {
			blip->SetColor({ 1.0f, 0.12f, 0.05f, 1.0f });
		} else {
			blip->SetColor({ 1.0f, 0.38f, 0.05f, 1.0f });
		}
		if (!drawVectorRadarMarkers) {
			blip->Update();
			blip->Draw();
		}

		const float relativeAltitude = enemyPosition.y - playerPosition.y;
		const float altitudeRatio = std::clamp(relativeAltitude / kAltitudeRange, -1.0f, 1.0f);
		const float altitudeY = radarCenter.y - altitudeRatio * altitudeScaleHalfHeight;
		if (drawVectorRadarMarkers) {
#ifdef ENABLE_IMGUI
			const Vector2 blipPosition = {
				radarCenter.x + (localRight * positionScale / kRadarRange) * radarRadius,
				radarCenter.y - (localForward * positionScale / kRadarRange) * radarRadius
			};
			const float triangleSize = enemy->IsBoss() ? 8.0f : 6.0f;
			const int markerAlpha = multiLockCount > 0
				? static_cast<int>(170.0f + 85.0f * multiLockPulse) : 255;
			const ImU32 enemyColor = IM_COL32(255, 54, 62, markerAlpha);

			// 通常の敵は常に赤い三角で表示する。先端はレーダー上の前方を向く。
			radarDrawList->AddTriangleFilled(
				toRadarOverlay({ blipPosition.x, blipPosition.y - triangleSize }),
				toRadarOverlay({ blipPosition.x - triangleSize * 0.82f, blipPosition.y + triangleSize * 0.72f }),
				toRadarOverlay({ blipPosition.x + triangleSize * 0.82f, blipPosition.y + triangleSize * 0.72f }),
				enemyColor);
			if (multiLockCount > 0) {
				// マルチロック済みは、三角を囲むリングで明確に区別する。
				const float ringRadius = triangleSize + 4.0f + multiLockPulse * 2.0f;
				radarDrawList->AddCircle(toRadarOverlay(blipPosition), ringRadius * radarOverlayWidth / screenWidth, enemyColor, 20, 2.0f);
			}

			// 高度目盛り上にも、対応する赤い横マーカーを出す。
			float displayAltitudeY = altitudeY;
			constexpr float kAltitudeMarkerMinimumSpacing = 10.0f;
			for (int attempt = 0; attempt < 12; ++attempt) {
				if (attempt > 0) {
					const float direction = (attempt % 2 == 0) ? 1.0f : -1.0f;
					const float distanceOffset = static_cast<float>((attempt + 1) / 2) * 12.0f;
					displayAltitudeY = std::clamp(
						altitudeY + direction * distanceOffset,
						radarCenter.y - altitudeScaleHalfHeight + 4.0f,
						radarCenter.y + altitudeScaleHalfHeight - 4.0f);
				}
				bool hasEnoughSpace = true;
				for (float placedY : placedAltitudeMarkerYs) {
					if (std::abs(displayAltitudeY - placedY) < kAltitudeMarkerMinimumSpacing) {
						hasEnoughSpace = false;
						break;
					}
				}
				if (hasEnoughSpace) {
					break;
				}
			}
			placedAltitudeMarkerYs.push_back(displayAltitudeY);
			const float altitudeMarkerWidth = multiLockCount > 0 ? 18.0f : 12.0f;
			radarDrawList->AddLine(
				toRadarOverlay({ altitudeScaleX + 3.0f, displayAltitudeY }),
				toRadarOverlay({ altitudeScaleX + altitudeMarkerWidth, displayAltitudeY }),
				enemyColor, multiLockCount > 0 ? 3.0f : 2.0f);
#endif
		} else {
			const float altitudeMarkerWidth = multiLockCount > 0 ? 16.0f : 10.0f;
			DrawAltitudeSpriteElement({ altitudeScaleX + altitudeMarkerWidth * 0.5f + 2.0f, altitudeY },
				{ altitudeMarkerWidth, multiLockCount > 0 ? 4.0f : 3.0f }, markerColor);
		}
	}
#ifdef ENABLE_IMGUI
	if (drawVectorRadarMarkers) {
		radarDrawList->PopClipRect();
	}
#endif
}
