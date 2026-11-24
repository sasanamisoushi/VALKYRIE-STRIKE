#include "PlayerPresentationController.h"

#include "3D/Model.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "engine/Camera/Camera.h"

#include <algorithm>
#include <cmath>

namespace {
Vector3 Add(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 Scale(const Vector3& v, float s) {
	return { v.x * s, v.y * s, v.z * s };
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
	};
}

Quaternion MakeLocalRotation(float rx, float ry, float rz) {
	Quaternion q = { 0.0f, 0.0f, 0.0f, 1.0f };
	if (std::abs(ry) > 0.0001f) q = MyMath::Multiply(MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, ry), q);
	if (std::abs(rx) > 0.0001f) q = MyMath::Multiply(MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, rx), q);
	if (std::abs(rz) > 0.0001f) q = MyMath::Multiply(MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, rz), q);
	return MyMath::Normalize(q);
}
} // namespace

void PlayerPresentationController::SetPresentationAlpha(float alpha) {
	presentationAlpha_ = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
}

void PlayerPresentationController::BeginModeTransition(int fromModeIndex) {
	transformFromModeIndex_ = std::clamp(fromModeIndex, 0, 2);
	transformModeBlend_ = 0.0f;
}

void PlayerPresentationController::SnapModeTransition() {
	transformModeBlend_ = 1.0f;
}

void PlayerPresentationController::InitializeGuardBarrier() {
	// プレイヤーの前方に展開する高視認性3Dエナジーシールドモデル
	ModelManager::GetInstance()->CreateSphereModel("PlayerGuardShieldDome", 32);
	if (Model* m = ModelManager::GetInstance()->FindModel("PlayerGuardShieldDome")) {
		m->SetTextureFilePath("resources/white1x1.png");
		m->SetEnableLighting(false);
		m->SetAlphaReference(0.0f);
		m->SetColor({ 0.2f, 0.85f, 1.0f, 0.85f });
	}
	guardBarrier_ = std::make_unique<Object3d>();
	guardBarrier_->Initialize(Object3dCommon::GetInstance());
	guardBarrier_->SetModel("PlayerGuardShieldDome");

	// リングは球ではなく、前方（ローカルZ方向）を向く平面リングとして生成する。
	ModelManager::GetInstance()->CreateRingModel("PlayerGuardShieldRing", 48, 1.0f, 0.82f);
	if (Model* m = ModelManager::GetInstance()->FindModel("PlayerGuardShieldRing")) {
		m->SetTextureFilePath("resources/white1x1.png");
		m->SetEnableLighting(false);
		m->SetAlphaReference(0.0f);
		m->SetColor({ 0.7f, 1.0f, 1.0f, 0.95f });
	}
	guardBarrierRing_ = std::make_unique<Object3d>();
	guardBarrierRing_->Initialize(Object3dCommon::GetInstance());
	guardBarrierRing_->SetModel("PlayerGuardShieldRing");
	guardBarrierPulse_ = 0.0f;
}

std::unique_ptr<Object3d> PlayerPresentationController::InitializeTransformMechModel() {
	ModelManager* modelManager = ModelManager::GetInstance();
	const auto prepareBox = [&](const char* name, const Vector4& color) {
		modelManager->CreateBoxModel(name);
		if (Model* model = modelManager->FindModel(name)) {
			model->SetTextureFilePath("resources/white1x1.png");
			model->SetEnableLighting(true);
			model->SetColor(color);
		}
	};
	const auto prepareCylinder = [&](const char* name, const Vector4& color) {
		modelManager->CreateCylinderModel(name, 12, 0.65f, 0.85f, 2.0f);
		if (Model* model = modelManager->FindModel(name)) {
			model->SetTextureFilePath("resources/white1x1.png");
			model->SetEnableLighting(true);
			model->SetColor(color);
		}
	};
	prepareBox("TransformMechArmor", { 0.82f, 0.86f, 0.91f, 1.0f });
	prepareBox("TransformMechFrame", { 0.06f, 0.08f, 0.12f, 1.0f });
	prepareBox("TransformMechAccent", { 0.86f, 0.04f, 0.06f, 1.0f });
	prepareBox("TransformMechVisor", { 0.03f, 0.90f, 0.72f, 1.0f });
	prepareCylinder("TransformMechEngine", { 0.10f, 0.12f, 0.16f, 1.0f });

	const auto pose = [](float x, float y, float z, float sx, float sy, float sz,
		float rx = 0.0f, float ry = 0.0f, float rz = 0.0f) {
		return TransformPlayerPose{
			{ x * 0.74f, y * 1.32f, z * 0.88f },
			{ sx * 0.70f, sy * 1.12f, sz * 0.78f },
			MakeLocalRotation(rx, ry, rz)
		};
	};
	const auto makeObject = [](const char* modelName) {
		auto object = std::make_unique<Object3d>();
		object->Initialize(Object3dCommon::GetInstance());
		object->SetModel(modelName);
		return object;
	};
	const auto addPart = [&](const char* name, const char* modelName,
		const TransformPlayerPose& fighter, const TransformPlayerPose& gerwalk, const TransformPlayerPose& battroid) {
		TransformPlayerPart part;
		part.name = name;
		part.object = makeObject(modelName);
		part.poses = { fighter, gerwalk, battroid };
		transformParts_.push_back(std::move(part));
	};

	transformParts_.clear();
	transformCore_.name = "Core";
	transformCore_.object = makeObject("TransformMechFrame");
	transformCore_.poses = {
		pose(0.0f, 0.0f, 0.0f, 5.0f, 1.6f, 8.0f),
		pose(0.0f, 3.8f, 0.0f, 3.8f, 1.6f, 4.8f),
		pose(0.0f, 5.2f, 0.0f, 2.6f, 2.0f, 1.6f)
	};
	std::unique_ptr<Object3d> coreObject = std::move(transformCore_.object);

	// 胴体・頭部
	addPart("Chest", "TransformMechArmor", pose(0, 0.8f, 1.2f, 4.6f, 1.4f, 3.2f), pose(0, 5.0f, 0.6f, 4.2f, 1.8f, 3.0f), pose(0, 7.2f, 0.0f, 4.2f, 2.1f, 2.2f));
	addPart("ChestAccent", "TransformMechAccent", pose(0, 1.0f, 3.4f, 0.65f, 0.65f, 2.0f), pose(0, 5.1f, 2.5f, 0.65f, 0.8f, 1.3f), pose(0, 7.4f, 1.9f, 0.65f, 1.0f, 0.25f));
	addPart("Head", "TransformMechArmor", pose(0, 0.8f, 4.0f, 1.35f, 1.05f, 1.5f), pose(0, 6.9f, 1.6f, 1.35f, 1.15f, 1.4f), pose(0, 10.0f, 0.0f, 1.35f, 1.35f, 1.35f));
	addPart("Visor", "TransformMechVisor", pose(0, 0.8f, 5.3f, 0.9f, 0.35f, 0.24f), pose(0, 6.9f, 2.95f, 0.9f, 0.35f, 0.24f), pose(0, 10.0f, 1.25f, 0.9f, 0.35f, 0.24f));
	addPart("Crest", "TransformMechAccent", pose(0, 1.8f, 4.0f, 0.3f, 1.5f, 0.6f), pose(0, 8.2f, 1.6f, 0.3f, 1.5f, 0.6f), pose(0, 11.7f, 0.0f, 0.3f, 1.5f, 0.6f));

	// 左右の腕
	addPart("LeftShoulder", "TransformMechArmor", pose(-5.0f, 0.0f, 0.4f, 2.3f, 0.75f, 4.8f, 0, 0, -0.18f), pose(-4.6f, 5.4f, 0.2f, 2.1f, 1.4f, 2.8f, 0, 0, -0.4f), pose(-5.0f, 7.5f, 0.0f, 2.0f, 1.7f, 1.8f));
	addPart("RightShoulder", "TransformMechArmor", pose(5.0f, 0.0f, 0.4f, 2.3f, 0.75f, 4.8f, 0, 0, 0.18f), pose(4.6f, 5.4f, 0.2f, 2.1f, 1.4f, 2.8f, 0, 0, 0.4f), pose(5.0f, 7.5f, 0.0f, 2.0f, 1.7f, 1.8f));
	addPart("LeftForearm", "TransformMechFrame", pose(-7.2f, -0.3f, 0.4f, 1.1f, 0.75f, 5.5f, 0, 0, -0.18f), pose(-5.9f, 3.2f, 1.2f, 1.25f, 1.1f, 3.0f, -0.45f, 0, -0.22f), pose(-6.0f, 4.4f, 0.8f, 1.25f, 2.8f, 1.25f, 0, 0, -0.08f));
	addPart("RightForearm", "TransformMechFrame", pose(7.2f, -0.3f, 0.4f, 1.1f, 0.75f, 5.5f, 0, 0, 0.18f), pose(5.9f, 3.2f, 1.2f, 1.25f, 1.1f, 3.0f, -0.45f, 0, 0.22f), pose(6.0f, 4.4f, 0.8f, 1.25f, 2.8f, 1.25f, 0, 0, 0.08f));
	addPart("LeftHand", "TransformMechArmor", pose(-8.0f, -0.4f, 2.6f, 1.2f, 0.7f, 1.8f), pose(-6.4f, 2.2f, 2.5f, 1.2f, 0.8f, 1.3f), pose(-6.0f, 1.2f, 1.0f, 1.15f, 1.0f, 1.0f));
	addPart("RightHand", "TransformMechArmor", pose(8.0f, -0.4f, 2.6f, 1.2f, 0.7f, 1.8f), pose(6.4f, 2.2f, 2.5f, 1.2f, 0.8f, 1.3f), pose(6.0f, 1.2f, 1.0f, 1.15f, 1.0f, 1.0f));

	// 脚部
	addPart("LeftHip", "TransformMechArmor", pose(-2.5f, -0.5f, -3.0f, 2.0f, 1.1f, 4.0f, -0.35f), pose(-2.2f, 1.8f, -1.8f, 1.9f, 1.8f, 2.7f, -0.3f), pose(-2.4f, 3.8f, 0.0f, 1.8f, 1.8f, 1.8f));
	addPart("RightHip", "TransformMechArmor", pose(2.5f, -0.5f, -3.0f, 2.0f, 1.1f, 4.0f, -0.35f), pose(2.2f, 1.8f, -1.8f, 1.9f, 1.8f, 2.7f, -0.3f), pose(2.4f, 3.8f, 0.0f, 1.8f, 1.8f, 1.8f));
	addPart("LeftShin", "TransformMechArmor", pose(-2.5f, -0.6f, -6.2f, 1.6f, 0.9f, 3.6f, -0.25f), pose(-2.2f, -1.1f, -2.4f, 1.45f, 3.4f, 1.5f, -0.12f), pose(-2.4f, -1.2f, 0.3f, 1.5f, 4.0f, 1.65f));
	addPart("RightShin", "TransformMechArmor", pose(2.5f, -0.6f, -6.2f, 1.6f, 0.9f, 3.6f, -0.25f), pose(2.2f, -1.1f, -2.4f, 1.45f, 3.4f, 1.5f, -0.12f), pose(2.4f, -1.2f, 0.3f, 1.5f, 4.0f, 1.65f));
	addPart("LeftFoot", "TransformMechFrame", pose(-2.5f, -0.8f, -9.0f, 1.7f, 0.65f, 2.7f), pose(-2.2f, -4.4f, -0.8f, 1.8f, 0.7f, 2.7f), pose(-2.4f, -5.5f, 1.5f, 1.9f, 0.75f, 2.8f));
	addPart("RightFoot", "TransformMechFrame", pose(2.5f, -0.8f, -9.0f, 1.7f, 0.65f, 2.7f), pose(2.2f, -4.4f, -0.8f, 1.8f, 0.7f, 2.7f), pose(2.4f, -5.5f, 1.5f, 1.9f, 0.75f, 2.8f));

	// 背面ユニット・主翼・推進器
	addPart("LeftWing", "TransformMechArmor", pose(-7.0f, 0.0f, -1.5f, 4.8f, 0.35f, 5.0f, 0, 0, -0.15f), pose(-5.5f, 4.4f, -1.8f, 3.8f, 0.5f, 3.8f, 0, 0, -0.45f), pose(-4.1f, 6.0f, -2.6f, 1.2f, 2.0f, 3.6f, 0, 0, -0.24f));
	addPart("RightWing", "TransformMechArmor", pose(7.0f, 0.0f, -1.5f, 4.8f, 0.35f, 5.0f, 0, 0, 0.15f), pose(5.5f, 4.4f, -1.8f, 3.8f, 0.5f, 3.8f, 0, 0, 0.45f), pose(4.1f, 6.0f, -2.6f, 1.2f, 2.0f, 3.6f, 0, 0, 0.24f));
	addPart("LeftEngine", "TransformMechEngine", pose(-2.6f, 0.1f, -6.5f, 1.4f, 2.8f, 1.4f, 1.57f), pose(-2.7f, 3.4f, -4.0f, 1.4f, 2.8f, 1.4f, 0.55f), pose(-2.8f, 6.0f, -3.5f, 1.4f, 2.8f, 1.4f, 0.18f));
	addPart("RightEngine", "TransformMechEngine", pose(2.6f, 0.1f, -6.5f, 1.4f, 2.8f, 1.4f, 1.57f), pose(2.7f, 3.4f, -4.0f, 1.4f, 2.8f, 1.4f, 0.55f), pose(2.8f, 6.0f, -3.5f, 1.4f, 2.8f, 1.4f, 0.18f));

	// コクピット・吸気口・装甲・ライフル
	addPart("CockpitCanopy", "TransformMechVisor", pose(0, 0.9f, 5.8f, 1.8f, 0.75f, 2.4f), pose(0, 6.8f, 3.5f, 1.6f, 0.8f, 1.8f), pose(0, 8.0f, 2.05f, 1.65f, 1.25f, 0.35f));
	addPart("LeftIntake", "TransformMechFrame", pose(-2.5f, 0.4f, 2.4f, 1.15f, 0.8f, 2.6f), pose(-2.4f, 5.0f, 1.5f, 1.05f, 1.1f, 1.6f), pose(-2.55f, 7.0f, 1.45f, 1.0f, 1.1f, 0.45f));
	addPart("RightIntake", "TransformMechFrame", pose(2.5f, 0.4f, 2.4f, 1.15f, 0.8f, 2.6f), pose(2.4f, 5.0f, 1.5f, 1.05f, 1.1f, 1.6f), pose(2.55f, 7.0f, 1.45f, 1.0f, 1.1f, 0.45f));
	addPart("LeftShoulderPod", "TransformMechArmor", pose(-5.5f, 0.4f, -3.6f, 2.0f, 1.1f, 3.2f, 0, 0, -0.24f), pose(-5.0f, 6.4f, -1.5f, 2.0f, 1.6f, 2.0f, 0, 0, -0.32f), pose(-5.25f, 8.7f, -1.4f, 2.15f, 1.7f, 1.55f, 0, 0, -0.16f));
	addPart("RightShoulderPod", "TransformMechArmor", pose(5.5f, 0.4f, -3.6f, 2.0f, 1.1f, 3.2f, 0, 0, 0.24f), pose(5.0f, 6.4f, -1.5f, 2.0f, 1.6f, 2.0f, 0, 0, 0.32f), pose(5.25f, 8.7f, -1.4f, 2.15f, 1.7f, 1.55f, 0, 0, 0.16f));
	addPart("LeftBicep", "TransformMechAccent", pose(-6.2f, -0.2f, 1.3f, 0.55f, 0.6f, 3.0f), pose(-5.2f, 4.2f, 0.7f, 0.65f, 1.7f, 0.7f, -0.3f), pose(-5.75f, 5.9f, 0.45f, 0.65f, 1.65f, 0.7f));
	addPart("RightBicep", "TransformMechAccent", pose(6.2f, -0.2f, 1.3f, 0.55f, 0.6f, 3.0f), pose(5.2f, 4.2f, 0.7f, 0.65f, 1.7f, 0.7f, -0.3f), pose(5.75f, 5.9f, 0.45f, 0.65f, 1.65f, 0.7f));
	addPart("Waist", "TransformMechFrame", pose(0, -0.2f, -1.8f, 2.8f, 0.85f, 2.5f), pose(0, 3.0f, -0.8f, 2.7f, 1.15f, 1.9f), pose(0, 4.8f, 0, 2.35f, 0.85f, 1.55f));
	addPart("FrontSkirt", "TransformMechArmor", pose(0, -0.7f, -3.9f, 2.6f, 0.7f, 1.3f), pose(0, 1.7f, 0.2f, 2.5f, 1.65f, 0.65f), pose(0, 3.9f, 1.5f, 2.7f, 1.55f, 0.5f));
	addPart("LeftSideSkirt", "TransformMechArmor", pose(-3.6f, -0.6f, -3.4f, 0.8f, 0.8f, 2.5f), pose(-3.1f, 1.8f, -0.2f, 0.8f, 1.55f, 1.35f), pose(-3.45f, 3.8f, 0.4f, 0.8f, 1.75f, 1.0f));
	addPart("RightSideSkirt", "TransformMechArmor", pose(3.6f, -0.6f, -3.4f, 0.8f, 0.8f, 2.5f), pose(3.1f, 1.8f, -0.2f, 0.8f, 1.55f, 1.35f), pose(3.45f, 3.8f, 0.4f, 0.8f, 1.75f, 1.0f));
	addPart("LeftKneeArmor", "TransformMechAccent", pose(-2.5f, -0.7f, -7.0f, 1.2f, 0.55f, 1.5f), pose(-2.2f, 0.3f, -1.2f, 1.15f, 0.9f, 0.65f), pose(-2.4f, 0.4f, 1.7f, 1.25f, 1.2f, 0.5f));
	addPart("RightKneeArmor", "TransformMechAccent", pose(2.5f, -0.7f, -7.0f, 1.2f, 0.55f, 1.5f), pose(2.2f, 0.3f, -1.2f, 1.15f, 0.9f, 0.65f), pose(2.4f, 0.4f, 1.7f, 1.25f, 1.2f, 0.5f));
	addPart("LeftCalfFin", "TransformMechArmor", pose(-3.7f, -0.6f, -7.5f, 0.55f, 0.55f, 3.0f, 0, 0, -0.25f), pose(-3.4f, -1.5f, -2.7f, 0.5f, 2.2f, 1.2f, 0, 0, -0.18f), pose(-3.75f, -1.5f, -0.8f, 0.45f, 2.45f, 1.4f, 0, 0, -0.16f));
	addPart("RightCalfFin", "TransformMechArmor", pose(3.7f, -0.6f, -7.5f, 0.55f, 0.55f, 3.0f, 0, 0, 0.25f), pose(3.4f, -1.5f, -2.7f, 0.5f, 2.2f, 1.2f, 0, 0, 0.18f), pose(3.75f, -1.5f, -0.8f, 0.45f, 2.45f, 1.4f, 0, 0, 0.16f));
	addPart("LeftWingStripe", "TransformMechAccent", pose(-7.0f, 0.45f, 0.8f, 3.7f, 0.18f, 0.4f, 0, 0, -0.15f), pose(-5.6f, 4.8f, -0.2f, 2.6f, 0.2f, 0.35f, 0, 0, -0.45f), pose(-4.2f, 6.3f, -0.2f, 0.65f, 1.55f, 0.32f, 0, 0, -0.24f));
	addPart("RightWingStripe", "TransformMechAccent", pose(7.0f, 0.45f, 0.8f, 3.7f, 0.18f, 0.4f, 0, 0, 0.15f), pose(5.6f, 4.8f, -0.2f, 2.6f, 0.2f, 0.35f, 0, 0, 0.45f), pose(4.2f, 6.3f, -0.2f, 0.65f, 1.55f, 0.32f, 0, 0, 0.24f));
	addPart("LeftAntenna", "TransformMechAccent", pose(-0.8f, 2.2f, 3.7f, 0.18f, 2.0f, 0.18f, 0, 0, -0.12f), pose(-0.8f, 9.0f, 1.5f, 0.18f, 2.1f, 0.18f, 0, 0, -0.12f), pose(-0.8f, 12.4f, -0.1f, 0.18f, 2.3f, 0.18f, 0, 0, -0.12f));
	addPart("RightAntenna", "TransformMechAccent", pose(0.8f, 2.2f, 3.7f, 0.18f, 2.0f, 0.18f, 0, 0, 0.12f), pose(0.8f, 9.0f, 1.5f, 0.18f, 2.1f, 0.18f, 0, 0, 0.12f), pose(0.8f, 12.4f, -0.1f, 0.18f, 2.3f, 0.18f, 0, 0, 0.12f));
	addPart("LeftChestPlate", "TransformMechArmor", pose(-1.9f, 1.1f, 4.1f, 1.35f, 0.9f, 1.1f, 0, 0, -0.30f), pose(-1.7f, 6.0f, 2.8f, 1.3f, 1.1f, 0.55f, 0, 0, -0.30f), pose(-1.6f, 7.5f, 2.2f, 1.3f, 1.35f, 0.32f, 0, 0, -0.30f));
	addPart("RightChestPlate", "TransformMechArmor", pose(1.9f, 1.1f, 4.1f, 1.35f, 0.9f, 1.1f, 0, 0, 0.30f), pose(1.7f, 6.0f, 2.8f, 1.3f, 1.1f, 0.55f, 0, 0, 0.30f), pose(1.6f, 7.5f, 2.2f, 1.3f, 1.35f, 0.32f, 0, 0, 0.30f));
	addPart("LeftHelmetCheek", "TransformMechArmor", pose(-1.15f, 1.0f, 5.0f, 0.25f, 0.6f, 0.55f), pose(-1.15f, 7.0f, 2.7f, 0.25f, 0.65f, 0.5f), pose(-1.15f, 10.0f, 0.95f, 0.25f, 0.85f, 0.42f));
	addPart("RightHelmetCheek", "TransformMechArmor", pose(1.15f, 1.0f, 5.0f, 0.25f, 0.6f, 0.55f), pose(1.15f, 7.0f, 2.7f, 0.25f, 0.65f, 0.5f), pose(1.15f, 10.0f, 0.95f, 0.25f, 0.85f, 0.42f));
	addPart("LeftToeArmor", "TransformMechArmor", pose(-2.5f, -0.9f, -10.7f, 1.5f, 0.4f, 1.4f), pose(-2.2f, -4.5f, 1.1f, 1.55f, 0.45f, 1.25f), pose(-2.4f, -5.5f, 3.5f, 1.7f, 0.45f, 1.3f));
	addPart("RightToeArmor", "TransformMechArmor", pose(2.5f, -0.9f, -10.7f, 1.5f, 0.4f, 1.4f), pose(2.2f, -4.5f, 1.1f, 1.55f, 0.45f, 1.25f), pose(2.4f, -5.5f, 3.5f, 1.7f, 0.45f, 1.3f));
	addPart("RifleBody", "TransformMechFrame", pose(0.0f, -0.8f, 8.0f, 0.7f, 0.7f, 6.8f), pose(-6.5f, 1.0f, 3.4f, 0.7f, 3.8f, 0.7f, 0, 0, -0.22f), pose(-7.2f, -0.4f, 2.2f, 0.7f, 4.8f, 0.7f, 0, 0, -0.18f));
	addPart("RifleBarrel", "TransformMechAccent", pose(0.0f, -0.8f, 13.6f, 0.26f, 0.26f, 1.6f), pose(-7.6f, -2.6f, 3.4f, 0.26f, 1.5f, 0.26f, 0, 0, -0.22f), pose(-8.6f, -5.0f, 2.2f, 0.26f, 1.7f, 0.26f, 0, 0, -0.18f));

	transformFromModeIndex_ = 0;
	transformModeBlend_ = 1.0f;
	return coreObject;
}

void PlayerPresentationController::UpdateTransformMech(
	Object3d* coreObject,
	int toModeIndex,
	const Vector3& position,
	const Quaternion& quaternion,
	const Vector3& modelScale,
	int dodgeTimer,
	int dodgeDurationFrames,
	float dodgeDirection,
	float guardPoseWeight) {
	transformModeBlend_ = (std::min)(1.0f, transformModeBlend_ + 0.10f);
	const int from = std::clamp(transformFromModeIndex_, 0, 2);
	const int to = std::clamp(toModeIndex, 0, 2);
	Quaternion bodyQuaternion = quaternion;
	if (dodgeTimer > 0 && dodgeDurationFrames > 0) {
		const float progress = 1.0f - static_cast<float>(dodgeTimer) / static_cast<float>(dodgeDurationFrames);
		const float rollAngle = progress * 6.2831853f * dodgeDirection;
		bodyQuaternion = MyMath::Normalize(MyMath::Multiply(
			quaternion, MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, rollAngle)));
	}

	const auto updateObject = [&](Object3d* object, const std::string& name,
		const std::array<TransformPlayerPose, 3>& poses) {
		if (!object) return;
		const TransformPlayerPose& fromPose = poses[from];
		const TransformPlayerPose& toPose = poses[to];
		Vector3 localPosition = Lerp(fromPose.translate, toPose.translate, transformModeBlend_);
		const Vector3 localScale = Lerp(fromPose.scale, toPose.scale, transformModeBlend_);
		const Quaternion localRotation = MyMath::Slerp(fromPose.rotate, toPose.rotate, transformModeBlend_);

		if (name == "LeftForearm" || name == "LeftHand") {
			localPosition.z += 2.8f * guardPoseWeight;
		}

		const Vector3 worldPosition = Add(position, MyMath::RotateVector(Scale(localPosition, modelScale.x), bodyQuaternion));
		object->SetTranslate(worldPosition);
		object->SetScale(Scale(localScale, modelScale.x));
		object->SetQuaternionRotate(MyMath::Normalize(MyMath::Multiply(localRotation, bodyQuaternion)));
		object->Update();
	};

	updateObject(coreObject, transformCore_.name, transformCore_.poses);
	for (TransformPlayerPart& part : transformParts_) {
		updateObject(part.object.get(), part.name, part.poses);
	}
}

void PlayerPresentationController::SetClipPlane(Object3d* coreObject, const Vector3& normal, float distance) {
	if (coreObject) {
		coreObject->SetWorldClipPlane(normal, distance);
	}
	for (TransformPlayerPart& part : transformParts_) {
		if (part.object) {
			part.object->SetWorldClipPlane(normal, distance);
		}
	}
}

void PlayerPresentationController::ClearClipPlane(Object3d* coreObject) {
	if (coreObject) {
		coreObject->ClearWorldClipPlane();
	}
	for (TransformPlayerPart& part : transformParts_) {
		if (part.object) {
			part.object->ClearWorldClipPlane();
		}
	}
}

void PlayerPresentationController::UpdateGuardBarrier(
	bool advanceState,
	float guardScale,
	int currentModeIndex,
	const Vector3& position,
	const Quaternion& quaternion,
	const Vector3& forward) {
	if (guardScale <= 0.01f) {
		return;
	}

	if (advanceState) {
		guardBarrierPulse_ += 0.15f;
	}
	const float pulse = 0.5f + 0.5f * std::sin(guardBarrierPulse_);
	const Vector3 up = MyMath::RotateVector({ 0.0f, 1.0f, 0.0f }, quaternion);

	float eyeHeight = 0.1f;
	if (currentModeIndex == 2) {
		eyeHeight = 0.8f;
	} else if (currentModeIndex == 1) {
		eyeHeight = 0.35f;
	}

	const Vector3 origin = {
		position.x + up.x * eyeHeight,
		position.y + up.y * eyeHeight,
		position.z + up.z * eyeHeight
	};
	const float forwardDistance = 0.80f;
	const Vector3 shieldPos = {
		origin.x + forward.x * forwardDistance,
		origin.y + forward.y * forwardDistance,
		origin.z + forward.z * forwardDistance
	};
	const float pulseScale = 1.0f + pulse * 0.06f;

	if (guardBarrier_) {
		const float shieldRadius = 1.10f * guardScale * pulseScale;
		const float shieldDepth = 0.30f * guardScale;
		guardBarrier_->SetTranslate(shieldPos);
		guardBarrier_->SetScale({ shieldRadius, shieldRadius, shieldDepth });
		guardBarrier_->SetQuaternionRotate(quaternion);
		if (guardBarrier_->GetModel()) {
			guardBarrier_->GetModel()->SetEnableLighting(false);
			guardBarrier_->GetModel()->SetTextureFilePath("resources/white1x1.png");
			guardBarrier_->GetModel()->SetColor({ 0.20f, 0.85f, 1.0f, (0.80f + pulse * 0.15f) * guardScale });
		}
		guardBarrier_->Update();
	}

	if (guardBarrierRing_) {
		const float ringRadius = 1.20f * guardScale * pulseScale;
		const float ringDepth = 1.0f;
		guardBarrierRing_->SetTranslate(shieldPos);
		guardBarrierRing_->SetScale({ ringRadius, ringRadius, ringDepth });
		const Quaternion ringRot = MyMath::Multiply(quaternion, MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, guardBarrierPulse_ * 0.6f));
		guardBarrierRing_->SetQuaternionRotate(ringRot);
		if (guardBarrierRing_->GetModel()) {
			guardBarrierRing_->GetModel()->SetEnableLighting(false);
			guardBarrierRing_->GetModel()->SetTextureFilePath("resources/white1x1.png");
			guardBarrierRing_->GetModel()->SetColor({ 0.80f, 1.0f, 1.0f, (0.95f + pulse * 0.05f) * guardScale });
		}
		guardBarrierRing_->Update();
	}
}

void PlayerPresentationController::DrawTransformParts(Camera* camera) const {
	for (const TransformPlayerPart& part : transformParts_) {
		if (part.object) {
			part.object->Update(camera);
			part.object->Draw();
		}
	}
}

void PlayerPresentationController::DrawGuardBarrier(Camera* camera, float guardScale) const {
	if (guardScale <= 0.01f) {
		return;
	}
	Object3dCommon::GetInstance()->SetAlphaBlendDrawSettings();
	if (guardBarrier_) {
		guardBarrier_->Update(camera);
		guardBarrier_->Draw();
	}
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
	if (guardBarrierRing_) {
		guardBarrierRing_->Update(camera);
		guardBarrierRing_->Draw();
	}
	Object3dCommon::GetInstance()->SetCommonDrawSettings();
}
