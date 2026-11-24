#include "DebugColliderRenderer.h"

#include "3D/Model.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/Player/Player.h"
#include "Game/bullet/MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"

#include <cmath>
#include <vector>

namespace {
Vector3 AddVector3(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

Vector3 ScaleVector3(const Vector3& v, float scale) {
	return { v.x * scale, v.y * scale, v.z * scale };
}

void PushLine(std::vector<VertexData>& vertices, const Vector3& start, const Vector3& end, const Vector4& color) {
	VertexData v1{};
	VertexData v2{};
	v1.position = { start.x, start.y, start.z, 1.0f };
	v2.position = { end.x, end.y, end.z, 1.0f };
	v1.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
	v2.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
	v1.texcoord = { 0.0f, 0.0f, 0.0f, 0.0f };
	v2.texcoord = { 1.0f, 1.0f, 0.0f, 0.0f };
	v1.color = color;
	v2.color = color;
	vertices.push_back(v1);
	vertices.push_back(v2);
}

void AddOBBShape(std::vector<VertexData>& vertices, const OBB& obb, const Vector4& color) {
	Vector3 p[8] = {
		AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
		AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
		AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
		AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2], -obb.size.z)),
		AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
		AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1], -obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
		AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0],  obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
		AddVector3(AddVector3(AddVector3(obb.center, ScaleVector3(obb.orientations[0], -obb.size.x)), ScaleVector3(obb.orientations[1],  obb.size.y)), ScaleVector3(obb.orientations[2],  obb.size.z)),
	};
	const int edges[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
	};
	for (const auto& edge : edges) {
		PushLine(vertices, p[edge[0]], p[edge[1]], color);
	}
}

void AddSphere(std::vector<VertexData>& vertices, const Vector3& center, float radius, const Vector4& color) {
	constexpr int segmentCount = 24;
	constexpr float twoPi = 6.283185307f;
	for (int i = 0; i < segmentCount; ++i) {
		const float angle1 = twoPi * static_cast<float>(i) / static_cast<float>(segmentCount);
		const float angle2 = twoPi * static_cast<float>(i + 1) / static_cast<float>(segmentCount);
		const float cos1 = std::cos(angle1);
		const float sin1 = std::sin(angle1);
		const float cos2 = std::cos(angle2);
		const float sin2 = std::sin(angle2);

		PushLine(vertices,
			{ center.x + radius * cos1, center.y + radius * sin1, center.z },
			{ center.x + radius * cos2, center.y + radius * sin2, center.z },
			color);
		PushLine(vertices,
			{ center.x, center.y + radius * cos1, center.z + radius * sin1 },
			{ center.x, center.y + radius * cos2, center.z + radius * sin2 },
			color);
		PushLine(vertices,
			{ center.x + radius * sin1, center.y, center.z + radius * cos1 },
			{ center.x + radius * sin2, center.y, center.z + radius * cos2 },
			color);
	}
}
} // namespace

void DebugColliderRenderer::Initialize() {
	ModelManager::GetInstance()->CreateLineModel("DebugColliderLines");
	colliderLinesObject_ = std::make_unique<Object3d>();
	colliderLinesObject_->Initialize(Object3dCommon::GetInstance());
	colliderLinesObject_->SetModel("DebugColliderLines");
}

void DebugColliderRenderer::Update(const DebugColliderContext& context) {
	if (!context.enabled || !context.updateWireframes || !colliderLinesObject_ || !colliderLinesObject_->GetModel()) {
		return;
	}

	std::vector<VertexData> colliderVertices;
	const bool drawAll = context.drawAllDebugFrames;
	const bool drawPlayer = drawAll || context.currentSimulationTarget == 0;
	const bool drawMissile = drawAll || context.currentSimulationTarget == 1;
	const bool drawEnemy = drawAll || context.currentSimulationTarget == 2;
	const bool drawObstacle = drawAll;

	if (drawPlayer && context.player && !context.player->IsDead()) {
		AddOBBShape(colliderVertices, context.player->GetOBB(), { 0.0f, 1.0f, 0.0f, 1.0f });
	}

	if (drawObstacle && context.obstacles) {
		for (const auto& obstacle : *context.obstacles) {
			if (!obstacle || obstacle->IsStageBounds()) {
				continue;
			}
			AddOBBShape(colliderVertices, obstacle->GetOBB(), { 0.0f, 1.0f, 1.0f, 1.0f });
		}
	}

	if (drawEnemy && context.enemies) {
		for (const auto& enemy : *context.enemies) {
			if (enemy && !enemy->IsDead()) {
				AddOBBShape(colliderVertices, enemy->GetOBB(), { 1.0f, 0.0f, 0.0f, 1.0f });
			}
		}
	}

	if ((drawEnemy || drawMissile) && context.lockedEnemy && !context.lockedEnemy->IsDead()) {
		AddSphere(colliderVertices, context.lockedEnemy->GetAimPosition(),
			context.lockedEnemy->GetCollisionRadius() + 0.35f, { 1.0f, 0.95f, 0.0f, 1.0f });
	}

	if (drawMissile && context.missileManager) {
		for (const auto& missile : context.missileManager->GetMissiles()) {
			if (missile && !missile->IsDead()) {
				AddSphere(colliderVertices, missile->GetPosition(), missile->GetCollisionRadius(), { 1.0f, 0.0f, 1.0f, 1.0f });
			}
		}
	}

	if (drawEnemy && context.enemyBulletManager) {
		for (const auto& bullet : context.enemyBulletManager->GetBullets()) {
			if (!bullet.isDead) {
				AddSphere(colliderVertices, bullet.position, 0.5f, { 1.0f, 0.5f, 0.0f, 1.0f });
			}
		}
	}

	if (colliderVertices.empty()) {
		VertexData v1{};
		VertexData v2{};
		v1.position = { 0.0f, 0.0f, 0.0f, 1.0f };
		v1.color = { 0.0f, 0.0f, 0.0f, 0.0f };
		v2.position = { 0.0f, 0.0f, 0.0f, 1.0f };
		v2.color = { 0.0f, 0.0f, 0.0f, 0.0f };
		colliderVertices.push_back(v1);
		colliderVertices.push_back(v2);
	}

	colliderLinesObject_->GetModel()->UpdateLineVertices(colliderVertices);
	colliderLinesObject_->Update();
}

void DebugColliderRenderer::Draw(bool enabled) const {
	if (enabled && colliderLinesObject_ && colliderLinesObject_->GetModel()) {
		colliderLinesObject_->Draw();
	}
}
