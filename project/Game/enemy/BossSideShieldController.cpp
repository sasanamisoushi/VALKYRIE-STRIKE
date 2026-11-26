#include "Boss.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr Vector3 kBossScale = { 1.85f, 1.55f, 1.85f };

constexpr std::array<Vector3, 2> kSideShieldPlaneCenters = {
	Vector3{ 2.5535f, 3.5352f, 0.0243f },
	Vector3{ -2.5535f, 3.5352f, 0.0243f },
};
constexpr float kSideShieldHeight = 5.1700f;
constexpr float kSideShieldChordLength = 7.3200f;
constexpr float kSideShieldArcRadius = kSideShieldChordLength / 1.41421356f;
constexpr float kSideShieldArcHalfAngle = 45.0f;
constexpr float kSideShieldAnchorOffset = 0.0800f;
constexpr float kSideShieldShellThickness = 0.18f;
constexpr int kSideShieldArcSubdivisions = 16;
constexpr int kSideShieldChargingFrames = 48;
constexpr int kSideShieldDeployFrames = 72;
constexpr int kSideShieldActiveFrames = 300;
constexpr int kSideShieldWarningFrames = 60;
constexpr int kSideShieldCooldownFrames = 420;
constexpr char kSideShieldTexturePath[] = "resources/boss_hex_shield_blue.png";
constexpr std::array<const char*, 4> kSideShieldModels = {
	"BossSideHexShieldOuterLeft", "BossSideHexShieldOuterRight",
	"BossSideHexShieldInnerLeft", "BossSideHexShieldInnerRight",
};
constexpr std::array<const char*, Boss::kSideShieldGeneratorCount> kSideShieldGeneratorModels = {
	// OBJ ローダーは X 軸を反転するため、ゲーム座標 +X 側（face 0）は OBJ の group 28。
	"Boss1/Boss1.obj#groups=28",
	"Boss1/Boss1.obj#groups=24",
};

Vector3 SubtractVector(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }

Quaternion MakeEulerQuaternion(const Vector3& euler) {
	const Quaternion qx = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, euler.x);
	const Quaternion qy = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, euler.y);
	const Quaternion qz = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, euler.z);
	return MyMath::Normalize(MyMath::Multiply(MyMath::Multiply(qx, qy), qz));
}

float SmoothStep01(float value) {
	value = std::clamp(value, 0.0f, 1.0f);
	return value * value * (3.0f - 2.0f * value);
}
}

void Boss::InitializeSideShields() {
	ModelManager* modelManager = ModelManager::GetInstance();
	for (const char* modelName : kSideShieldModels) {
		// 初期状態はほぼ閉じた円弧。Update() で左右から開く角度へ更新する。
		modelManager->CreateCylinderArcShellModel(modelName, kSideShieldArcSubdivisions,
			kSideShieldArcRadius, kSideShieldHeight, kSideShieldShellThickness, 45.0f, 45.1f);
		Model* model = modelManager->FindModel(modelName);
		if (!model) continue;
		model->SetTextureFilePath(kSideShieldTexturePath);
		// 曲面の法線へ光を当て、カメラ角度によっても円弧の陰影を見せる。
		model->SetEnableLighting(true);
		model->SetDoubleSidedLighting(true);
		model->SetShininess(24.0f);
		model->SetAlphaReference(0.0f);
		model->SetColor({ 0.30f, 0.78f, 1.00f, 0.0f });
	}

	for (size_t index = 0; index < sideShieldSegments_.size(); ++index) {
		sideShieldSegments_[index] = std::make_unique<Object3d>();
		sideShieldSegments_[index]->Initialize(Object3dCommon::GetInstance());
		sideShieldSegments_[index]->SetModel(kSideShieldModels[index]);
	}

	for (size_t face = 0; face < Boss::kSideShieldGeneratorCount; ++face) {
		const char* modelName = kSideShieldGeneratorModels[face];
		if (!modelManager->FindModel(modelName)) {
			modelManager->LoadModel(modelName);
		}
		sideShieldGenerators_[face] = std::make_unique<Object3d>();
		sideShieldGenerators_[face]->Initialize(Object3dCommon::GetInstance());
		sideShieldGenerators_[face]->SetModel(modelName);
		if (Model* model = sideShieldGenerators_[face]->GetModel()) {
			model->SetEnableLighting(true);
			model->SetShininess(112.0f);
			model->SetColor({ 0.25f, 0.48f, 0.56f, 1.0f });
		}
	}
}

bool Boss::IsSideShieldVisible() const {
	return sideShieldState_ == Boss::SideShieldState::Deploying ||
		sideShieldState_ == Boss::SideShieldState::Active ||
		sideShieldState_ == Boss::SideShieldState::Warning;
}

bool Boss::IsSideShieldProtectingFace(size_t face) const {
	return face < Boss::kSideShieldGeneratorCount && !sideShieldGeneratorDestroyed_[face] && IsSideShieldVisible();
}

bool Boss::IsSideShieldGeneratorVulnerable(size_t face) const {
	return face < Boss::kSideShieldGeneratorCount && !sideShieldGeneratorDestroyed_[face] &&
		(sideShieldState_ == Boss::SideShieldState::Charging || sideShieldState_ == Boss::SideShieldState::Cooldown);
}

bool Boss::ApplySideShieldGeneratorDamage(size_t face, int damage) {
	if (!IsSideShieldGeneratorVulnerable(face) || damage <= 0) return false;
	sideShieldGeneratorHP_[face] = (std::max)(0, sideShieldGeneratorHP_[face] - damage);
	sideShieldGeneratorDestroyed_[face] = sideShieldGeneratorHP_[face] == 0;
	return true;
}

bool Boss::GetActiveSideShieldImpact(
	const Vector3& hitPosition,
	float impactRadius,
	Vector3& outNormal) const {
	size_t face = 0;
	if (!TryGetSideShieldFaceAtWorldPosition(hitPosition, impactRadius, face) ||
		!IsSideShieldProtectingFace(face)) {
		return false;
	}

	const float outwardDirection = face == 0 ? 1.0f : -1.0f;
	outNormal = MyMath::RotateVector({ outwardDirection, 0.0f, 0.0f }, MakeEulerQuaternion(GetRotation()));
	return true;
}

bool Boss::TryGetSideShieldFaceAtWorldPosition(
	const Vector3& worldPosition,
	float radius,
	size_t& outFace) const {
	const Vector3 relative = SubtractVector(worldPosition, GetPosition());
	const Quaternion hull = MakeEulerQuaternion(GetRotation());
	const Quaternion inverseHull = { -hull.x, -hull.y, -hull.z, hull.w };
	const Vector3 scaledLocal = MyMath::RotateVector(relative, inverseHull);
	const Vector3 local = {
		scaledLocal.x / kBossScale.x,
		scaledLocal.y / kBossScale.y,
		scaledLocal.z / kBossScale.z,
	};
	const float localRadius = radius / (std::min)(kBossScale.x, (std::min)(kBossScale.y, kBossScale.z));

	for (size_t face = 0; face < Boss::kSideShieldGeneratorCount; ++face) {
		const Vector3& center = kSideShieldPlaneCenters[face];
		const float outwardDirection = face == 0 ? 1.0f : -1.0f;
		const float outwardDistance = (local.x - center.x) * outwardDirection;
		const bool withinHeight = std::abs(local.y - center.y) <= kSideShieldHeight * 0.55f + localRadius;
		const bool withinLength = std::abs(local.z - center.z) <= kSideShieldChordLength * 0.55f + localRadius;
		// 弾はシールドを通過した後に船体側の OBB で着弾位置が確定する。
		// その位置でも防御を判定できるよう、発生装置から船体表面までを側面防御領域に含める。
		const bool withinArcDepth = outwardDistance >= -2.10f - localRadius &&
			outwardDistance <= kSideShieldArcRadius * 0.42f + localRadius;
		if (withinHeight && withinLength && withinArcDepth) {
			outFace = face;
			return true;
		}
	}
	return false;
}

void Boss::AdvanceSideShieldState() {
	const bool hasGenerator = std::any_of(sideShieldGeneratorDestroyed_.begin(), sideShieldGeneratorDestroyed_.end(),
		[](bool destroyed) { return !destroyed; });
	if (!hasGenerator) {
		sideShieldState_ = Boss::SideShieldState::Cooldown;
		sideShieldStateTimer_ = 0;
		return;
	}

	++sideShieldStateTimer_;
	int duration = kSideShieldChargingFrames;
	switch (sideShieldState_) {
	case Boss::SideShieldState::Charging: duration = kSideShieldChargingFrames; break;
	case Boss::SideShieldState::Deploying: duration = kSideShieldDeployFrames; break;
	case Boss::SideShieldState::Active: duration = kSideShieldActiveFrames; break;
	case Boss::SideShieldState::Warning: duration = kSideShieldWarningFrames; break;
	case Boss::SideShieldState::Cooldown: duration = kSideShieldCooldownFrames; break;
	}
	if (sideShieldStateTimer_ < duration) return;

	sideShieldStateTimer_ = 0;
	switch (sideShieldState_) {
	case Boss::SideShieldState::Charging: sideShieldState_ = Boss::SideShieldState::Deploying; break;
	case Boss::SideShieldState::Deploying: sideShieldState_ = Boss::SideShieldState::Active; break;
	case Boss::SideShieldState::Active: sideShieldState_ = Boss::SideShieldState::Warning; break;
	case Boss::SideShieldState::Warning: sideShieldState_ = Boss::SideShieldState::Cooldown; break;
	case Boss::SideShieldState::Cooldown: sideShieldState_ = Boss::SideShieldState::Charging; break;
	}
}

void Boss::UpdateSideShields() {
	AdvanceSideShieldState();

	if (sideShieldState_ == Boss::SideShieldState::Deploying) {
		sideShieldDeployProgress_ = SmoothStep01(
			static_cast<float>(sideShieldStateTimer_) / static_cast<float>(kSideShieldDeployFrames));
	} else if (sideShieldState_ == Boss::SideShieldState::Active) {
		sideShieldDeployProgress_ = 1.0f;
	} else if (sideShieldState_ == Boss::SideShieldState::Warning) {
		// 解除予告の点滅と同時に、外端から中央へ畳んで消す。
		sideShieldDeployProgress_ = 1.0f - SmoothStep01(
			static_cast<float>(sideShieldStateTimer_) / static_cast<float>(kSideShieldWarningFrames));
	} else {
		sideShieldDeployProgress_ = 0.0f;
	}

	float alpha = tuning_.sideShieldOpacity;
	Vector4 shieldColor = { 0.30f, 0.78f, 1.00f, alpha };
	Vector4 generatorColor = { 0.25f, 0.48f, 0.56f, 1.0f };
	switch (sideShieldState_) {
	case Boss::SideShieldState::Charging: {
		const float pulse = 0.62f + 0.38f * std::sin(static_cast<float>(sideShieldStateTimer_) * 0.40f);
		generatorColor = { 0.20f + 0.30f * pulse, 0.64f + 0.28f * pulse, 0.92f + 0.08f * pulse, 1.0f };
		break;
	}
	case Boss::SideShieldState::Deploying:
		alpha *= 0.35f + 0.65f * sideShieldDeployProgress_;
		generatorColor = { 0.24f, 0.74f, 1.00f, 1.0f };
		break;
	case Boss::SideShieldState::Active:
		generatorColor = { 0.18f, 0.68f, 1.00f, 1.0f };
		break;
	case Boss::SideShieldState::Warning: {
		const float blink = 0.25f + 0.75f * (0.5f + 0.5f * std::sin(static_cast<float>(sideShieldStateTimer_) * 1.30f));
		alpha *= blink;
		generatorColor = blink > 0.62f
			? Vector4{ 1.00f, 0.34f, 0.06f, 1.0f }
			: Vector4{ 0.16f, 0.62f, 1.00f, 1.0f };
		break;
	}
	case Boss::SideShieldState::Cooldown:
		generatorColor = { 1.00f, 0.18f, 0.035f, 1.0f };
		break;
	}

	const float arcAngle = kSideShieldArcHalfAngle * (std::max)(0.001f, sideShieldDeployProgress_);
	const float endpointInset = kSideShieldArcRadius * 0.70710678f;
	const bool needsMeshRebuild = std::abs(sideShieldBuiltProgress_ - sideShieldDeployProgress_) > 0.0001f;
	if (needsMeshRebuild) sideShieldBuiltProgress_ = sideShieldDeployProgress_;
	const Quaternion hullQuaternion = MakeEulerQuaternion(GetRotation());

	for (size_t face = 0; face < Boss::kSideShieldGeneratorCount; ++face) {
		Object3d* generator = sideShieldGenerators_[face].get();
		if (generator && !sideShieldGeneratorDestroyed_[face]) {
			if (Model* model = generator->GetModel()) {
				model->SetColor(generatorColor);
			}
			// 切り出した OBJ はボス原点基準の頂点座標を持つため、船体と同じ変換だけを適用する。
			generator->SetTranslate(GetPosition());
			generator->SetQuaternionRotate(hullQuaternion);
			generator->SetScale(kBossScale);
			generator->Update();
		}

		if (sideShieldGeneratorDestroyed_[face]) continue;
		const float outwardDirection = face == 0 ? 1.0f : -1.0f;
		for (size_t half = 0; half < 2; ++half) {
			Object3d* segment = sideShieldSegments_[face * 2 + half].get();
			if (!segment) continue;

			// 2枚とも中央から外端へ広げる。Warning では progress が 1→0 になるため、
			// 逆に外端から中央へ収束して消える。
			const float centerAngle = outwardDirection < 0.0f ? 90.0f : 270.0f;
			const float startAngle = half == 0 ? centerAngle - arcAngle : centerAngle;
			const float endAngle = half == 0 ? centerAngle : centerAngle + arcAngle;

			Model* model = ModelManager::GetInstance()->FindModel(kSideShieldModels[face * 2 + half]);
			if (model) {
				if (needsMeshRebuild) {
					model->InitializeCylinderShell(model->GetModelCommon(), kSideShieldArcSubdivisions, 6,
						kSideShieldArcRadius, kSideShieldHeight, kSideShieldShellThickness,
						startAngle, endAngle);
				}
				model->SetTextureFilePath(kSideShieldTexturePath);
				model->SetEnableLighting(true);
				model->SetDoubleSidedLighting(true);
				model->SetShininess(24.0f);
				model->SetAlphaReference(0.0f);
				model->SetColor({ shieldColor.x, shieldColor.y, shieldColor.z, alpha });
			}

			Vector3 localPosition = kSideShieldPlaneCenters[face];
			localPosition.x += outwardDirection * (kSideShieldAnchorOffset - endpointInset);
			localPosition.y -= kSideShieldHeight * 0.5f;
			segment->SetTranslate(LocalPointToWorld(localPosition));
			segment->SetRotate({ 0.0f, GetRotation().y, 0.0f });
			segment->SetScale(kBossScale);
			segment->Update();
		}
	}
}

void Boss::DrawSideShields() {
	if (!IsSideShieldVisible() || sideShieldDeployProgress_ <= 0.001f) return;

	Object3dCommon* object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->SetAlphaBlendDrawSettings();
	for (size_t face = 0; face < Boss::kSideShieldGeneratorCount; ++face) {
		if (sideShieldGeneratorDestroyed_[face]) continue;
		for (size_t half = 0; half < 2; ++half) {
			if (const std::unique_ptr<Object3d>& segment = sideShieldSegments_[face * 2 + half]) {
				segment->Draw();
			}
		}
	}
	object3dCommon->SetCommonDrawSettings();
}
