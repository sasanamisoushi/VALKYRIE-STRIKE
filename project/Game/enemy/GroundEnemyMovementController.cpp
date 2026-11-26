#include "GroundEnemy.h"
#include "Game/obstacle/Obstacle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {
constexpr float kGroundSpeed = 0.08f;
constexpr float kTrackSpeed = 0.16f;
constexpr float kQuadrupedEngageDistance = 28.0f;
constexpr float kModelForwardYaw = 1.57079632679f;
constexpr std::array<Vector3, 4> kLegPivots = {
	Vector3{ -0.62f, 1.35f, 0.76f }, Vector3{ -0.62f, 1.35f, -0.76f },
	Vector3{ 0.80f, 1.35f, -0.76f }, Vector3{ 0.80f, 1.35f, 0.76f }
};
constexpr std::array<Vector3, 4> kFootPositions = {
	Vector3{ -1.48f, 0.0f, 0.76f }, Vector3{ -1.48f, 0.0f, -0.76f },
	Vector3{ 0.98f, 0.0f, -0.76f }, Vector3{ 0.98f, 0.0f, 0.76f }
};

Vector3 NormalizeOrFallback(const Vector3& v, const Vector3& fallback) {
	float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
	if (lenSq <= 0.0001f) {
		return fallback;
	}
	float len = std::sqrt(lenSq);
	return { v.x / len, v.y / len, v.z / len };
}

bool GetTriangleY(const Vector3& p0, const Vector3& p1, const Vector3& p2, float x, float z, float& outY) {
	float denom = (p1.z - p2.z) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.z - p2.z);
	if (std::abs(denom) < 0.00001f) return false;

	float w0 = ((p1.z - p2.z) * (x - p2.x) + (p2.x - p1.x) * (z - p2.z)) / denom;
	float w1 = ((p2.z - p0.z) * (x - p2.x) + (p0.x - p2.x) * (z - p2.z)) / denom;
	float w2 = 1.0f - w0 - w1;

	if (w0 >= -0.01f && w1 >= -0.01f && w2 >= -0.01f) {
		outY = w0 * p0.y + w1 * p1.y + w2 * p2.y;
		return true;
	}
	return false;
}

bool SampleTerrainHeight(const std::list<std::unique_ptr<Obstacle>>& obstacles, const Vector3& probePosition, float& outHeight) {
	Sphere probe{};
	probe.center = probePosition;
	probe.radius = 1.5f;
	bool foundGround = false;
	float highest = -99999.0f;

	for (const auto& obstacle : obstacles) {
		if (!obstacle || obstacle->IsStageBounds() || !obstacle->IsCollisionEnabled() || !obstacle->IsUseMeshCollider()) {
			continue;
		}

		const auto& triangles = obstacle->GetWorldTriangles();
		for (const size_t triangleIndex : obstacle->GetNearbyWorldTriangleIndices(probe)) {
			const Triangle& triangle = triangles[triangleIndex];
			if (std::abs(triangle.normal.y) <= 0.1f) continue;

			float height = 0.0f;
			if (GetTriangleY(triangle.p[0], triangle.p[1], triangle.p[2], probePosition.x, probePosition.z, height) && height > highest) {
				highest = height;
				foundGround = true;
			}
		}
	}

	if (foundGround) outHeight = highest;
	return foundGround;
}
}

void GroundEnemy::UpdateGroundMovement(
	const Vector3& playerPos,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) {
	const Vector3 scale = GetScale();
	Vector3 pos = GetPosition();

	// 重力計算（落下）
	velocityY_ -= gravity_;
	pos.y += velocityY_;

	// プレイヤーと完全に同等のメッシュコライダー（球 vs メッシュ三角形交差・押し出し）当たり判定
	Sphere enemySphere;
	enemySphere.center = pos;
	enemySphere.radius = scale.y;

	bool hitGroundThisFrame = false;

	for (const auto& obs : obstacles) {
		if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled()) continue;

		if (obs->IsUseMeshCollider()) {
			const auto& triangles = obs->GetWorldTriangles();
			const auto& nearbyTriangleIndices = obs->GetNearbyWorldTriangleIndices(enemySphere);

			float maxTerrainY = -99999.0f;
			bool foundTerrain = false;
			for (const size_t triangleIndex : nearbyTriangleIndices) {
				const Triangle& tri = triangles[triangleIndex];
				if (std::abs(tri.normal.y) > 0.2f) {
					float triY = 0.0f;
					if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], pos.x, pos.z, triY)) {
						if (triY > maxTerrainY) {
							maxTerrainY = triY;
							foundTerrain = true;
						}
					}
				}
			}

			const float footOffset = GetFootOffset();
			if (foundTerrain && pos.y < maxTerrainY + footOffset) {
				pos.y = maxTerrainY + footOffset;
				if (velocityY_ < 0.0f) {
					velocityY_ = 0.0f;
				}
				enemySphere.center = pos;
				hitGroundThisFrame = true;
			}

			for (const size_t triangleIndex : nearbyTriangleIndices) {
				const Triangle& tri = triangles[triangleIndex];
				if (std::abs(tri.normal.y) > 0.3f) {
					continue;
				}
				Vector3 pushVector;
				if (MyMath::IsCollision(enemySphere, tri, pushVector)) {
					pos.x += pushVector.x;
					pos.y += pushVector.y;
					pos.z += pushVector.z;
					enemySphere.center = pos;
				}
			}
		} else {
			OBB obsOBB = obs->GetOBB();
			if (std::abs(pos.x - obsOBB.center.x) <= obsOBB.size.x + scale.x * 0.5f &&
				std::abs(pos.z - obsOBB.center.z) <= obsOBB.size.z + scale.z * 0.5f) {
				float topY = obsOBB.center.y + obsOBB.size.y;
				if (pos.y <= topY + scale.y) {
					pos.y = topY + scale.y;
					hitGroundThisFrame = true;
				}
			}
		}
	}

	SetPosition(pos);

	if (hitGroundThisFrame) {
		if (velocityY_ < 0.0f) {
			velocityY_ = 0.0f;
		}
		isGrounded_ = true;
	} else {
		isGrounded_ = false;
	}

	Vector3 vel = GetVelocity();
	// スクリプト等で追跡を停止された場合だけ、Blenderで配置した位置を保つ。
	if (!IsChasingPlayer()) {
		vel.x = 0.0f;
		vel.z = 0.0f;
		SetVelocity(vel);
		UpdateTerrainPose(obstacles);
		return;
	}

	Vector3 toPlayerHorizontal = { playerPos.x - pos.x, 0.0f, playerPos.z - pos.z };
	float distHorizontalSq = toPlayerHorizontal.x * toPlayerHorizontal.x + toPlayerHorizontal.z * toPlayerHorizontal.z;

	if (distHorizontalSq > 0.001f) {
		float distH = std::sqrt(distHorizontalSq);
		Vector3 targetDir = { toPlayerHorizontal.x / distH, 0.0f, toPlayerHorizontal.z / distH };

		locomotionMode_ = (attackSubState_ != GroundAttackState::Melee && distH > kQuadrupedEngageDistance)
			? LocomotionMode::Track
			: LocomotionMode::Quadruped;
		const float targetTrackBlend = locomotionMode_ == LocomotionMode::Track ? 1.0f : 0.0f;
		trackBlend_ += (targetTrackBlend - trackBlend_) * 0.12f;

		const float turnRate = 0.08f - trackBlend_ * 0.05f;
		Vector3 fwd = GetForward();
		fwd.x += (targetDir.x - fwd.x) * turnRate;
		fwd.z += (targetDir.z - fwd.z) * turnRate;
		fwd = NormalizeOrFallback(fwd, { 0.0f, 0.0f, 1.0f });
		SetForward(fwd);

		SetRotationAngles({ 0.0f, std::atan2(fwd.x, fwd.z), 0.0f });

		if (attackSubState_ == GroundAttackState::Melee) {
			vel.x = 0.0f;
			vel.z = 0.0f;
		} else {
			constexpr float kMinKeepDistance = 15.0f;
			const float moveSpeed = kGroundSpeed + (kTrackSpeed - kGroundSpeed) * trackBlend_;
			const float directionSign = distHorizontalSq < kMinKeepDistance * kMinKeepDistance ? -0.5f : 1.0f;
			if (trackBlend_ > 0.01f) {
				const Vector3 targetVelocity = { fwd.x * moveSpeed * directionSign, 0.0f, fwd.z * moveSpeed * directionSign };
				const float traction = 0.08f + (1.0f - trackBlend_) * 0.20f;
				vel.x += (targetVelocity.x - vel.x) * traction;
				vel.z += (targetVelocity.z - vel.z) * traction;
			} else if (distHorizontalSq < kMinKeepDistance * kMinKeepDistance) {
				vel.x = -fwd.x * kGroundSpeed * 0.5f;
				vel.z = -fwd.z * kGroundSpeed * 0.5f;
			} else {
				vel.x = fwd.x * kGroundSpeed;
				vel.z = fwd.z * kGroundSpeed;
			}
		}
	}

	if (!isGrounded_) {
		vel.x *= 0.98f;
		vel.z *= 0.98f;
	}

	SetVelocity(vel);
	pos.x += vel.x;
	pos.z += vel.z;
	SetPosition(pos);

	const bool isAirborneAttack = attackSubState_ == GroundAttackState::Melee &&
		(meleePhase_ == MeleePhase::Rising || meleePhase_ == MeleePhase::Falling);
	if (!isAirborneAttack) {
		Sphere terrainProbe{};
		terrainProbe.center = pos;
		terrainProbe.radius = (std::max)(0.5f, (std::max)(scale.x, scale.z));

		float groundHeight = -99999.0f;
		bool foundGroundAtDestination = false;
		for (const auto& obs : obstacles) {
			if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled() || !obs->IsUseMeshCollider()) {
				continue;
			}

			const auto& triangles = obs->GetWorldTriangles();
			for (const size_t triangleIndex : obs->GetNearbyWorldTriangleIndices(terrainProbe)) {
				const Triangle& tri = triangles[triangleIndex];
				if (std::abs(tri.normal.y) <= 0.1f) {
					continue;
				}

				float terrainY = 0.0f;
				if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], pos.x, pos.z, terrainY) && terrainY > groundHeight) {
					groundHeight = terrainY;
					foundGroundAtDestination = true;
				}
			}
		}

		if (foundGroundAtDestination) {
			groundY_ = groundHeight;
			pos.y = groundY_ + GetFootOffset();
			SetPosition(pos);
			velocityY_ = 0.0f;
			isGrounded_ = true;
		}
	}

	UpdateTerrainPose(obstacles);
}

void GroundEnemy::UpdateTerrainPose(const std::list<std::unique_ptr<Obstacle>>& obstacles) {
	const Vector3 pos = GetPosition();
	const Vector3 fwd = GetForward();
	const Vector3 rot = GetRotation();
	const Vector3 scale = GetScale();

	Vector3 flatForward = NormalizeOrFallback({ fwd.x, 0.0f, fwd.z }, { 0.0f, 0.0f, 1.0f });
	Vector3 right = { flatForward.z, 0.0f, -flatForward.x };
	constexpr float kProbeDistance = 1.0f;
	float front = 0.0f, back = 0.0f, left = 0.0f, rightHeight = 0.0f;
	const bool hasFront = SampleTerrainHeight(obstacles, { pos.x + flatForward.x * kProbeDistance, pos.y, pos.z + flatForward.z * kProbeDistance }, front);
	const bool hasBack = SampleTerrainHeight(obstacles, { pos.x - flatForward.x * kProbeDistance, pos.y, pos.z - flatForward.z * kProbeDistance }, back);
	const bool hasLeft = SampleTerrainHeight(obstacles, { pos.x - right.x * kProbeDistance, pos.y, pos.z - right.z * kProbeDistance }, left);
	const bool hasRight = SampleTerrainHeight(obstacles, { pos.x + right.x * kProbeDistance, pos.y, pos.z + right.z * kProbeDistance }, rightHeight);

	const float pitchTarget = hasFront && hasBack ? std::atan2(front - back, kProbeDistance * 2.0f) : 0.0f;
	const float rollTarget = hasLeft && hasRight ? std::atan2(rightHeight - left, kProbeDistance * 2.0f) : 0.0f;
	terrainPitch_ += (pitchTarget - terrainPitch_) * 0.16f;
	terrainRoll_ += (rollTarget - terrainRoll_) * 0.16f;

	const bool hasTerrain = hasFront || hasBack || hasLeft || hasRight;
	if (hasTerrain) {
		const Vector3 terrainRotation = { terrainRoll_, rot.y + kModelForwardYaw, terrainPitch_ };
		const Matrix4x4 terrainMatrix = MyMath::MakeAffineMatrix(
			{ 1.0f, 1.0f, 1.0f }, terrainRotation, { 0.0f, 0.0f, 0.0f });
		const float footOffset = GetFootOffset();
		float lowestSupportY = 0.0f;
		for (const Vector3& foot : kFootPositions) {
			const Vector3 rotatedFoot = MyMath::Transform({
				foot.x * scale.x,
				-footOffset,
				foot.z * scale.z,
			}, terrainMatrix);
			lowestSupportY = (std::min)(lowestSupportY, rotatedFoot.y);
		}
		const float clearanceTarget = std::clamp(-footOffset - lowestSupportY, 0.0f, 0.75f) + 0.025f;
		terrainClearance_ += (clearanceTarget - terrainClearance_) * 0.28f;
	} else {
		terrainClearance_ *= 0.72f;
	}

	for (float& legOffset : legGroundOffsets_) {
		legOffset += (0.0f - legOffset) * 0.35f;
	}
}

void GroundEnemy::SnapToGround(const std::list<std::unique_ptr<Obstacle>>& obstacles) {
	Vector3 pos = GetPosition();
	float maxGroundY = -99999.0f;
	bool foundGround = false;

	for (const auto& obs : obstacles) {
		if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled()) continue;

		if (obs->IsUseMeshCollider()) {
			const auto& triangles = obs->GetWorldTriangles();
			Sphere terrainSample{};
			terrainSample.center = pos;
			terrainSample.radius = 0.25f;
			for (const size_t triangleIndex : obs->GetNearbyWorldTriangleIndices(terrainSample)) {
				const Triangle& tri = triangles[triangleIndex];
				if (std::abs(tri.normal.y) > 0.1f) {
					float triY = 0.0f;
					if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], pos.x, pos.z, triY)) {
						if (triY > maxGroundY) {
							maxGroundY = triY;
							foundGround = true;
						}
					}
				}
			}
		}
	}

	if (!foundGround) {
		float offsets[8][2] = {
			{ -3.0f, 0.0f }, { 3.0f, 0.0f }, { 0.0f, -3.0f }, { 0.0f, 3.0f },
			{ -6.0f, -6.0f }, { 6.0f, -6.0f }, { -6.0f, 6.0f }, { 6.0f, 6.0f }
		};
		for (int i = 0; i < 8; ++i) {
			float sampleX = pos.x + offsets[i][0];
			float sampleZ = pos.z + offsets[i][1];
			for (const auto& obs : obstacles) {
				if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled()) continue;
				if (obs->IsUseMeshCollider()) {
					const auto& triangles = obs->GetWorldTriangles();
					Sphere terrainSample{};
					terrainSample.center = { sampleX, pos.y, sampleZ };
					terrainSample.radius = 0.25f;
					for (const size_t triangleIndex : obs->GetNearbyWorldTriangleIndices(terrainSample)) {
						const Triangle& tri = triangles[triangleIndex];
						if (std::abs(tri.normal.y) > 0.1f) {
							float triY = 0.0f;
							if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], sampleX, sampleZ, triY)) {
								if (triY > maxGroundY) {
									maxGroundY = triY;
									foundGround = true;
								}
							}
						}
					}
				}
			}
			if (foundGround) break;
		}
	}

	if (!foundGround) {
		for (const auto& obs : obstacles) {
			if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled() || !obs->IsUseMeshCollider()) {
				continue;
			}

			for (const Triangle& tri : obs->GetWorldTriangles()) {
				if (std::abs(tri.normal.y) <= 0.1f) {
					continue;
				}

				float triY = 0.0f;
				if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], pos.x, pos.z, triY) && triY > maxGroundY) {
					maxGroundY = triY;
					foundGround = true;
				}
			}
		}
	}

	const float footOffset = GetFootOffset();
	if (foundGround) {
		pos.y = maxGroundY + footOffset;
	} else {
		pos.y = (std::max)(pos.y, footOffset);
	}
	SetPosition(pos);

	velocityY_ = 0.0f;
	isGrounded_ = true;
	UpdateModel();
}

float GroundEnemy::GetFootOffset() const {
	const Object3d* obj = GetObject3d();
	const Vector3 scale = GetScale();
	if (!obj || !obj->GetModel()) {
		return scale.y;
	}

	const Vector3 center = obj->GetModel()->GetBoundsCenter();
	const Vector3 halfExtents = obj->GetModel()->GetHalfExtents();
	float localBottomY = center.y - halfExtents.y;

	for (size_t i = 0; i < legs_.size(); ++i) {
		if (!legs_[i] || !legs_[i]->GetModel()) {
			continue;
		}
		const Vector3 legCenter = legs_[i]->GetModel()->GetBoundsCenter();
		const Vector3 legHalfExtents = legs_[i]->GetModel()->GetHalfExtents();
		localBottomY = (std::min)(localBottomY, kLegPivots[i].y + legCenter.y - legHalfExtents.y);
	}
	return (std::max)(0.0f, -localBottomY * std::abs(scale.y)) + 0.025f;
}

void GroundEnemy::UpdateFlightPathMovement() {
	Vector3 targetPos{};
	if (!AdvanceGroundFlightPath(targetPos)) {
		return;
	}

	const Vector3 oldPosition = GetPosition();
	Vector3 nextPos = oldPosition;
	nextPos.x = targetPos.x;
	nextPos.z = targetPos.z;
	SetPosition(nextPos);

	const Vector3 diff = { nextPos.x - oldPosition.x, 0.0f, nextPos.z - oldPosition.z };
	const float diffLen = std::sqrt(diff.x * diff.x + diff.z * diff.z);
	if (diffLen > 0.001f) {
		const Vector3 fwd = { diff.x / diffLen, 0.0f, diff.z / diffLen };
		SetForward(fwd);
		Vector3 rot = GetRotation();
		rot.y = std::atan2(fwd.x, fwd.z);
		SetRotationAngles(rot);
	}

	Vector3 vel = GetVelocity();
	vel.x = 0.0f;
	vel.z = 0.0f;
	SetVelocity(vel);
}
