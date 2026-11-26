#include "Boss.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
Vector3 SubtractVector(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 ScaleVector(const Vector3& v, float s) { return { v.x * s, v.y * s, v.z * s }; }
float DotVector(const Vector3& a, const Vector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vector3 NormalizeVector(const Vector3& value) {
	const float length = std::sqrt(DotVector(value, value));
	return length > 0.0001f ? ScaleVector(value, 1.0f / length) : Vector3{ 0.0f, 0.0f, 1.0f };
}

Quaternion MakeEulerQuaternion(const Vector3& euler) {
	const Quaternion qx = MyMath::MakeAxisAngle({ 1.0f, 0.0f, 0.0f }, euler.x);
	const Quaternion qy = MyMath::MakeAxisAngle({ 0.0f, 1.0f, 0.0f }, euler.y);
	const Quaternion qz = MyMath::MakeAxisAngle({ 0.0f, 0.0f, 1.0f }, euler.z);
	return MyMath::Normalize(MyMath::Multiply(MyMath::Multiply(qx, qy), qz));
}
}

void Boss::SetCinematicWreckPresentation(float burnRatio) {
	cinematicWreckPresentation_ = true;
	const float burn = std::clamp(burnRatio, 0.0f, 1.0f);
	const Vector4 hullColor = {
		0.20f * (1.0f - burn) + 0.025f * burn,
		0.33f * (1.0f - burn) + 0.018f * burn,
		0.39f * (1.0f - burn) + 0.014f * burn,
		1.0f,
	};
	const Vector4 wreckPartColor = {
		0.29f * (1.0f - burn) + 0.035f * burn,
		0.43f * (1.0f - burn) + 0.022f * burn,
		0.48f * (1.0f - burn) + 0.018f * burn,
		1.0f,
	};
	if (Object3d* obj = GetObject3d(); obj && obj->GetModel()) obj->GetModel()->SetColor(hullColor);
	for (const auto& hullPart : cinematicHullDebris_) {
		if (hullPart && hullPart->GetModel()) hullPart->GetModel()->SetColor(hullColor);
	}
	for (const auto& turret : mainTurrets_) {
		if (turret && turret->GetModel()) turret->GetModel()->SetColor(wreckPartColor);
	}
	for (const auto& turret : miniTurretBases_) {
		if (turret && turret->GetModel()) turret->GetModel()->SetColor(wreckPartColor);
	}
	for (const auto& barrel : miniTurretBarrels_) {
		if (barrel && barrel->GetModel()) barrel->GetModel()->SetColor(wreckPartColor);
	}
	if (machineGunTurret_ && machineGunTurret_->GetModel()) machineGunTurret_->GetModel()->SetColor(wreckPartColor);
	for (const auto& turret : forwardTurrets_) {
		if (turret && turret->GetModel()) turret->GetModel()->SetColor(wreckPartColor);
	}
}

void Boss::BeginCinematicDebris() {
	if (cinematicDebrisActive_) return;

	const Vector3 bossCenter = GetBodyLockOnPosition();
	const OBB bossObb = GetOBB();
	static std::mt19937 debrisRng(std::random_device{}());
	std::uniform_real_distribution<float> distSigned(-1.0f, 1.0f);
	std::uniform_real_distribution<float> distScaleXZ(0.28f, 0.92f);
	std::uniform_real_distribution<float> distScaleY(0.10f, 0.42f);
	std::uniform_real_distribution<float> distPower(18.0f, 34.0f);
	std::uniform_real_distribution<float> distRot(0.0f, 6.2831853f);
	constexpr float kSpinPower = 10.5f;

	cinematicDebrisParts_.fill(nullptr);
	cinematicDebrisVelocities_.fill({});
	cinematicDebrisAngularVelocities_.fill({});
	cinematicSmallDebrisResting_.fill(false);

	// 大型の船体・砲塔ブロックは描画せず、爆発を覆わない細かいデブリだけを使う。

	const size_t thrusterCount = GetRearThrusterCount();
	for (size_t i = 0; i < cinematicSmallDebris_.size(); ++i) {
		Object3d* part = cinematicSmallDebris_[i].get();
		if (!part) continue;

		// 半数はエンジン付近、残りは船体各所の内部から飛び散らせる
		Vector3 initialPos{};
		if (thrusterCount > 0 && (i % 2 == 0)) {
			const Vector3 enginePos = GetRearThrusterWorldPosition(i % thrusterCount);
			initialPos = {
				enginePos.x + distSigned(debrisRng) * 1.4f,
				enginePos.y + distSigned(debrisRng) * 0.9f,
				enginePos.z + distSigned(debrisRng) * 1.4f,
			};
		} else {
			const float rx = distSigned(debrisRng) * bossObb.size.x * 0.75f;
			const float ry = distSigned(debrisRng) * bossObb.size.y * 0.75f;
			const float rz = distSigned(debrisRng) * bossObb.size.z * 0.75f;
			initialPos = {
				bossObb.center.x + bossObb.orientations[0].x * rx + bossObb.orientations[1].x * ry + bossObb.orientations[2].x * rz,
				bossObb.center.y + bossObb.orientations[0].y * rx + bossObb.orientations[1].y * ry + bossObb.orientations[2].y * rz,
				bossObb.center.z + bossObb.orientations[0].z * rx + bossObb.orientations[1].z * ry + bossObb.orientations[2].z * rz,
			};
		}

		part->SetTranslate(initialPos);
		part->SetQuaternionRotate(MakeEulerQuaternion({ distRot(debrisRng), distRot(debrisRng), distRot(debrisRng) }));
		part->SetScale({ distScaleXZ(debrisRng), distScaleY(debrisRng), distScaleXZ(debrisRng) });
		part->Update();

		// デブリごとの放射状ベクトルの計算: Normalize(Debris.InitialPos - Boss.CenterPos) + ランダムばらつき
		Vector3 offset = SubtractVector(initialPos, bossCenter);
		offset.x += distSigned(debrisRng) * 1.8f;
		offset.y += distSigned(debrisRng) * 1.2f;
		offset.z += distSigned(debrisRng) * 1.8f;
		const float len = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
		Vector3 direction{};
		if (len > 0.05f) {
			direction = { offset.x / len, offset.y / len, offset.z / len };
		} else {
			const float angle = static_cast<float>(i) * 2.39996323f;
			direction = NormalizeVector({ std::cos(angle), 0.45f, std::sin(angle) });
		}

		const float explosionPower = distPower(debrisRng);
		// すべてを空の彼方へ飛ばさず、約 1/7 は地面に残る焼けた残骸にする。
		const bool leavesWreckage = (i % 7) == 0;
		const float upwardDir = std::abs(direction.y) * 0.85f + 0.30f;
		cinematicSmallDebrisVelocities_[i] = {
			direction.x * explosionPower * (leavesWreckage ? 0.32f : 1.0f),
			leavesWreckage ? 2.0f + std::abs(direction.y) * 3.5f : upwardDir * explosionPower + 5.0f,
			direction.z * explosionPower * (leavesWreckage ? 0.32f : 1.0f),
		};
		cinematicSmallDebrisAngularVelocities_[i] = {
			distSigned(debrisRng) * kSpinPower,
			distSigned(debrisRng) * kSpinPower,
			distSigned(debrisRng) * kSpinPower,
		};
	}

	cinematicDebrisActive_ = true;

	// 船体および砲塔をすべて非表示にし、細かい破片のみを描画する。
	if (Object3d* obj = GetObject3d()) {
		obj->SetScale({ 0.0f, 0.0f, 0.0f });
		obj->Update();
	}
}

void Boss::SetCinematicDebrisRestPlane(float worldY) {
	cinematicDebrisRestPlaneY_ = worldY;
	hasCinematicDebrisRestPlane_ = true;
}

void Boss::StepCinematicDebris(float deltaTime) {
	if (!cinematicDebrisActive_ || deltaTime <= 0.0f) return;
	// 高速で飛び散る細かいデブリへ重力を適用し、爆発後に自然な放物線で落下させる。
	constexpr float kDebrisGravity = 16.0f;
	for (size_t index = 0; index < cinematicDebrisParts_.size(); ++index) {
		Object3d* part = cinematicDebrisParts_[index];
		if (!part) continue;
		Vector3 position = part->GetTranslate();
		cinematicDebrisVelocities_[index].y -= kDebrisGravity * deltaTime;
		position.x += cinematicDebrisVelocities_[index].x * deltaTime;
		position.y += cinematicDebrisVelocities_[index].y * deltaTime;
		position.z += cinematicDebrisVelocities_[index].z * deltaTime;
		const Vector3 deltaEuler = {
			cinematicDebrisAngularVelocities_[index].x * deltaTime,
			cinematicDebrisAngularVelocities_[index].y * deltaTime,
			cinematicDebrisAngularVelocities_[index].z * deltaTime,
		};
		part->SetTranslate(position);
		part->SetQuaternionRotate(MyMath::Normalize(
			MyMath::Multiply(MakeEulerQuaternion(deltaEuler), part->GetQuaternionRotate())));
		part->Update();
	}
	for (size_t index = 0; index < cinematicSmallDebris_.size(); ++index) {
		Object3d* part = cinematicSmallDebris_[index].get();
		if (!part) continue;
		if (cinematicSmallDebrisResting_[index]) continue;
		Vector3 position = part->GetTranslate();
		cinematicSmallDebrisVelocities_[index].y -= kDebrisGravity * deltaTime;
		position.x += cinematicSmallDebrisVelocities_[index].x * deltaTime;
		position.y += cinematicSmallDebrisVelocities_[index].y * deltaTime;
		position.z += cinematicSmallDebrisVelocities_[index].z * deltaTime;
		const Vector3 deltaEuler = {
			cinematicSmallDebrisAngularVelocities_[index].x * deltaTime,
			cinematicSmallDebrisAngularVelocities_[index].y * deltaTime,
			cinematicSmallDebrisAngularVelocities_[index].z * deltaTime,
		};
		const Quaternion nextRot = MyMath::Normalize(
			MyMath::Multiply(MakeEulerQuaternion(deltaEuler), part->GetQuaternionRotate()));
		if (hasCinematicDebrisRestPlane_ && (index % 7) == 0 &&
			cinematicSmallDebrisVelocities_[index].y < 0.0f && position.y <= cinematicDebrisRestPlaneY_) {
			position.y = cinematicDebrisRestPlaneY_;
			cinematicSmallDebrisVelocities_[index] = {};
			cinematicSmallDebrisAngularVelocities_[index] = {};
			cinematicSmallDebrisResting_[index] = true;
		}
		part->SetTranslate(position);
		part->SetQuaternionRotate(nextRot);
		part->Update();
	}
}

std::vector<Vector3> Boss::GetCinematicWreckageWorldPositions() const {
	std::vector<Vector3> positions;
	if (!cinematicDebrisActive_) return positions;
	positions.reserve(cinematicSmallDebris_.size() / 7 + 1);
	for (size_t i = 0; i < cinematicSmallDebris_.size(); ++i) {
		if (cinematicSmallDebrisResting_[i] && cinematicSmallDebris_[i]) {
			positions.push_back(cinematicSmallDebris_[i]->GetTranslate());
		}
	}
	return positions;
}

std::vector<Vector3> Boss::GetCinematicDebrisWorldPositions() const {
	std::vector<Vector3> positions;
	if (!cinematicDebrisActive_) return positions;
	positions.reserve(cinematicDebrisParts_.size() + 14);
	for (const Object3d* part : cinematicDebrisParts_) {
		if (part) positions.push_back(part->GetTranslate());
	}
	for (size_t i = 0; i < cinematicSmallDebris_.size(); i += 3) {
		Object3d* part = cinematicSmallDebris_[i].get();
		if (part) {
			positions.push_back(part->GetTranslate());
		}
	}
	return positions;
}
