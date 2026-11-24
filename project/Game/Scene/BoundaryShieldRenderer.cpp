#include "BoundaryShieldRenderer.h"

#include "BoundaryShieldSettingsManager.h"
#include "3D/Model.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"
#include "engine/Resource/TextureManager.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr char kBoundaryShieldTexturePath[] = "resources/boundary_hex_shield.png";

float SmoothStep01(float value) {
	const float t = std::clamp(value, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
} // namespace

void BoundaryShieldRenderer::Initialize() {
	pulseTime_ = 0.0f;
	TextureManager::GetInstance()->LoadTexture(kBoundaryShieldTexturePath);
	ModelManager::GetInstance()->CreatePlaneModel("BoundaryHexShield");
	Model* alertModel = ModelManager::GetInstance()->FindModel("BoundaryHexShield");
	if (alertModel) {
		alertModel->SetTextureFilePath(kBoundaryShieldTexturePath);
		// テクスチャの透明部分は捨て、六角形の発光線だけを半透明で重ねる。
		alertModel->SetAlphaReference(0.01f);
		alertModel->SetEnableLighting(false);
		alertModel->SetShininess(0.0f);
	}
	boundaryAlertObject_ = std::make_unique<Object3d>();
	boundaryAlertObject_->Initialize(Object3dCommon::GetInstance());
	boundaryAlertObject_->SetModel("BoundaryHexShield");
	ceilingBoundaryAlertObject_ = std::make_unique<Object3d>();
	ceilingBoundaryAlertObject_->Initialize(Object3dCommon::GetInstance());
	ceilingBoundaryAlertObject_->SetModel("BoundaryHexShield");
}

void BoundaryShieldRenderer::DrawAlertSurface(
	Object3d* alertObject,
	const Vector3& position,
	const Vector3& normal,
	float intensity,
	float pulseAlpha,
	const BoundaryShieldSettings& settings) const {
	if (!alertObject) {
		return;
	}

	const bool isCeiling = normal.y > 0.5f;
	alertObject->SetScale(isCeiling
		? Vector3{ settings.ceilingWidth, settings.ceilingHeight, 1.0f }
		: Vector3{ settings.wallWidth, settings.wallHeight, 1.0f });

	Vector3 rotate = { 0.0f, std::atan2(normal.x, normal.z), 0.0f };
	if (isCeiling) {
		rotate = { -1.570796f, 0.0f, 0.0f };
	}

	if (Model* m = alertObject->GetModel()) {
		const float fade = SmoothStep01(intensity);
		m->SetColor({
			settings.color.x,
			settings.color.y,
			settings.color.z,
			settings.maxOpacity * fade * pulseAlpha
		});
	}

	alertObject->SetTranslate({
		position.x + normal.x * settings.surfaceOffset,
		position.y + normal.y * settings.surfaceOffset,
		position.z + normal.z * settings.surfaceOffset
	});

	alertObject->SetRotate(rotate);
	alertObject->Update();
	Object3dCommon::GetInstance()->SetAlphaBlendDrawSettings();
	alertObject->Draw();
}

void BoundaryShieldRenderer::Draw(bool enabled, const Player* player, const BoundaryShieldSettings& settings) {
	if (!enabled || !player || !boundaryAlertObject_ || !ceilingBoundaryAlertObject_) {
		return;
	}

	pulseTime_ += settings.pulseSpeed;
	// フェードイン後も、かすかな脈動でエネルギー場らしい存在感を保つ。
	const float pulseAlpha = (1.0f - settings.pulseStrength) +
		settings.pulseStrength * std::sin(pulseTime_);

	if (player->IsNearWallBoundary()) {
		DrawAlertSurface(
			boundaryAlertObject_.get(),
			player->GetWallBoundaryAlertPosition(),
			player->GetWallBoundaryAlertNormal(),
			player->GetWallBoundaryWarningIntensity(),
			pulseAlpha,
			settings);
	}
	if (player->IsNearCeilingBoundary()) {
		DrawAlertSurface(
			ceilingBoundaryAlertObject_.get(),
			player->GetCeilingBoundaryAlertPosition(),
			player->GetCeilingBoundaryAlertNormal(),
			player->GetCeilingBoundaryWarningIntensity(),
			pulseAlpha,
			settings);
	}
	if (player->IsNearBoundary()) {
		Object3dCommon::GetInstance()->SetCommonDrawSettings();
	}
}
