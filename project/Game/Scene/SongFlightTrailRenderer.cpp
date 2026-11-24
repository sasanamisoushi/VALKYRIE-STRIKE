#include "SongFlightTrailRenderer.h"

#include "3D/Model.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"
#include "engine/Camera/Camera.h"

#include <algorithm>
#include <string>

namespace {
Vector3 AddVector3(const Vector3& a, const Vector3& b) {
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 ScaleVector3(const Vector3& v, float scale) {
	return { v.x * scale, v.y * scale, v.z * scale };
}

Vector3 NormalizeOrVector3(const Vector3& v, const Vector3& fallback) {
	const float length = MyMath::Length(v);
	if (length <= 0.0001f) {
		return fallback;
	}
	return { v.x / length, v.y / length, v.z / length };
}
}

void SongFlightTrailRenderer::Initialize() {
	isInitialized_ = false;
	heads_.fill({ 0.0f, 0.0f, 0.0f });

	// 歌の残像はノズルの炎と共有せず、胴体と両翼端の飛行履歴を別々に保持する。
	for (size_t index = 0; index < kTrailCount; ++index) {
		const std::string modelName = "SongFlightTrail" + std::to_string(index);
		ModelManager::GetInstance()->CreateTrailModel(modelName);
		trails_[index] = std::make_unique<Trail>();
		trails_[index]->Initialize(128);
		objects_[index] = std::make_unique<Object3d>();
		objects_[index]->Initialize(Object3dCommon::GetInstance());
		objects_[index]->SetModel(modelName);
	}
}

void SongFlightTrailRenderer::Reset() {
	isInitialized_ = false;
}

void SongFlightTrailRenderer::Update(const Player& player) {
	// ワールド上方向から右軸を作るとロール時に翼端が追従しないため、
	// OBBのローカル3軸をそのまま使う。これでピッチ・ヨー・ロールを
	// すべて含めた機体上の正しい取付位置になる。
	const OBB playerObb = player.GetOBB();
	const Vector3 right = NormalizeOrVector3(playerObb.orientations[0], { 1.0f, 0.0f, 0.0f });
	const Vector3 up = NormalizeOrVector3(playerObb.orientations[1], { 0.0f, 1.0f, 0.0f });
	const Vector3 forward = NormalizeOrVector3(playerObb.orientations[2], { 0.0f, 0.0f, 1.0f });
	const Vector3 halfExtents = playerObb.size;
	const float wingSpan = (std::max)(halfExtents.x * 0.82f, 0.55f);
	const float bodyLength = (std::max)(halfExtents.z, 0.50f);
	const Vector3 bodyAnchor = AddVector3(playerObb.center, AddVector3(
		ScaleVector3(up, halfExtents.y * 0.14f), ScaleVector3(forward, -bodyLength * 0.20f)));

	heads_[0] = bodyAnchor;
	heads_[1] = AddVector3(bodyAnchor, ScaleVector3(right, wingSpan));
	heads_[2] = AddVector3(bodyAnchor, ScaleVector3(right, -wingSpan));

	for (size_t index = 0; index < kTrailCount; ++index) {
		if (!trails_[index]) {
			continue;
		}
		if (!isInitialized_) {
			trails_[index]->SetOrigin(heads_[index]);
		} else {
			trails_[index]->Update(heads_[index], 1.0f / 60.0f, 0.72f);
		}
	}
	isInitialized_ = true;
}

void SongFlightTrailRenderer::Draw(Camera* renderCamera, const Player& player, int songFrame) {
	if (!renderCamera) {
		return;
	}

	const Vector3 forward = NormalizeOrVector3(player.GetForwardVector(), { 0.0f, 0.0f, 1.0f });
	// スラスターより控えめな薄い黄の残像にし、機体を隠さない。
	const float baseThickness = std::clamp(player.GetCollisionRadius() * 0.032f, 0.028f, 0.085f);
	Object3dCommon::GetInstance()->SetSpeedTrailDrawSettings();
	for (size_t index = 0; index < kTrailCount; ++index) {
		Trail* trail = trails_[index].get();
		Object3d* trailObject = objects_[index].get();
		if (!trail || !trailObject || !trailObject->GetModel()) {
			continue;
		}
		const float width = index == 0 ? baseThickness * 1.15f : baseThickness;
		const std::vector<VertexData> vertices = trail->GenerateSpeedTrailVertices(
			renderCamera, width, heads_[index],
			{ -forward.x, -forward.y, -forward.z }, 0.50f, 0.72f,
			{ 1.0f, 0.54f, 0.04f, 0.50f }, { 1.0f, 0.90f, 0.24f, 0.60f });
		if (vertices.empty()) {
			continue;
		}
		trailObject->GetModel()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		trailObject->GetModel()->SetUvTransform(MyMath::MakeAffineMatrix(
			Vector3{ 1.0f, 4.5f, 1.0f }, Vector3{ 0.0f, 0.0f, 0.0f },
			Vector3{ 0.0f, -static_cast<float>(songFrame) * 0.018f, 0.0f }));
		trailObject->GetModel()->UpdateTrailVertices(vertices, true);
		trailObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
		trailObject->SetScale({ 1.0f, 1.0f, 1.0f });
		trailObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
		trailObject->Update(renderCamera);
		trailObject->Draw();
	}
	Object3dCommon::GetInstance()->SetEffectDrawSettings();
}
