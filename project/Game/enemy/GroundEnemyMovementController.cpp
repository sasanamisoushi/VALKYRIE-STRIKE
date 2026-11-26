#include "GroundEnemyMovementController.h"

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

size_t ResolvePathIndex(int index, size_t pointCount, bool loop) {
	if (pointCount == 0) return 0;
	if (loop) {
		int count = static_cast<int>(pointCount);
		int wrapped = index % count;
		if (wrapped < 0) wrapped += count;
		return static_cast<size_t>(wrapped);
	}
	int last = static_cast<int>(pointCount - 1);
	return static_cast<size_t>(std::clamp(index, 0, last));
}

Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
	float t2 = t * t;
	float t3 = t2 * t;
	return {
		0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
		0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
		0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t + (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 + (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3)
	};
}

Vector3 EvaluateFlightPath(const std::vector<Vector3>& points, size_t segmentIndex, float t, bool loop) {
	int baseIndex = static_cast<int>(segmentIndex);
	const Vector3& p0 = points[ResolvePathIndex(baseIndex - 1, points.size(), loop)];
	const Vector3& p1 = points[ResolvePathIndex(baseIndex, points.size(), loop)];
	const Vector3& p2 = points[ResolvePathIndex(baseIndex + 1, points.size(), loop)];
	const Vector3& p3 = points[ResolvePathIndex(baseIndex + 2, points.size(), loop)];
	return CatmullRom(p0, p1, p2, p3, t);
}
}

void GroundEnemyMovementController::UpdateGroundMovement(
	GroundEnemy& enemy,
	const Vector3& playerPos,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	// 重力計算（落下）
	enemy.velocityY_ -= enemy.gravity_;
	enemy.position_.y += enemy.velocityY_;

	// プレイヤーと完全に同等のメッシュコライダー（球 vs メッシュ三角形交差・押し出し）当たり判定
	Sphere enemySphere;
	enemySphere.center = enemy.position_;
	enemySphere.radius = enemy.scale_.y;

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
					if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], enemy.position_.x, enemy.position_.z, triY)) {
						if (triY > maxTerrainY) {
							maxTerrainY = triY;
							foundTerrain = true;
						}
					}
				}
			}

			const float footOffset = GetFootOffset(enemy);
			if (foundTerrain && enemy.position_.y < maxTerrainY + footOffset) {
				enemy.position_.y = maxTerrainY + footOffset;
				if (enemy.velocityY_ < 0.0f) {
					enemy.velocityY_ = 0.0f;
				}
				enemySphere.center = enemy.position_;
				hitGroundThisFrame = true;
			}

			for (const size_t triangleIndex : nearbyTriangleIndices) {
				const Triangle& tri = triangles[triangleIndex];
				if (std::abs(tri.normal.y) > 0.3f) {
					continue;
				}
				Vector3 pushVector;
				if (MyMath::IsCollision(enemySphere, tri, pushVector)) {
					enemy.position_.x += pushVector.x;
					enemy.position_.y += pushVector.y;
					enemy.position_.z += pushVector.z;
					enemySphere.center = enemy.position_;
				}
			}
		} else {
			OBB obsOBB = obs->GetOBB();
			if (std::abs(enemy.position_.x - obsOBB.center.x) <= obsOBB.size.x + enemy.scale_.x * 0.5f &&
				std::abs(enemy.position_.z - obsOBB.center.z) <= obsOBB.size.z + enemy.scale_.z * 0.5f) {
				float topY = obsOBB.center.y + obsOBB.size.y;
				if (enemy.position_.y <= topY + enemy.scale_.y) {
					enemy.position_.y = topY + enemy.scale_.y;
					hitGroundThisFrame = true;
				}
			}
		}
	}

	if (hitGroundThisFrame) {
		if (enemy.velocityY_ < 0.0f) {
			enemy.velocityY_ = 0.0f;
		}
		enemy.isGrounded_ = true;
	} else {
		enemy.isGrounded_ = false;
	}

	// スクリプト等で追跡を停止された場合だけ、Blenderで配置した位置を保つ。
	if (!enemy.isChasingPlayer_) {
		enemy.velocity_.x = 0.0f;
		enemy.velocity_.z = 0.0f;
		UpdateTerrainPose(enemy, obstacles);
		return;
	}

	Vector3 toPlayerHorizontal = { playerPos.x - enemy.position_.x, 0.0f, playerPos.z - enemy.position_.z };
	float distHorizontalSq = toPlayerHorizontal.x * toPlayerHorizontal.x + toPlayerHorizontal.z * toPlayerHorizontal.z;

	if (distHorizontalSq > 0.001f) {
		float distH = std::sqrt(distHorizontalSq);
		Vector3 targetDir = { toPlayerHorizontal.x / distH, 0.0f, toPlayerHorizontal.z / distH };

		enemy.locomotionMode_ = (enemy.attackSubState_ != GroundEnemy::GroundAttackState::Melee && distH > kQuadrupedEngageDistance)
			? GroundEnemy::LocomotionMode::Track
			: GroundEnemy::LocomotionMode::Quadruped;
		const float targetTrackBlend = enemy.locomotionMode_ == GroundEnemy::LocomotionMode::Track ? 1.0f : 0.0f;
		enemy.trackBlend_ += (targetTrackBlend - enemy.trackBlend_) * 0.12f;

		const float turnRate = 0.08f - enemy.trackBlend_ * 0.05f;
		enemy.forward_.x += (targetDir.x - enemy.forward_.x) * turnRate;
		enemy.forward_.z += (targetDir.z - enemy.forward_.z) * turnRate;
		enemy.forward_ = NormalizeOrFallback(enemy.forward_, { 0.0f, 0.0f, 1.0f });

		enemy.rotation_.y = std::atan2(enemy.forward_.x, enemy.forward_.z);
		enemy.rotation_.x = 0.0f;
		enemy.rotation_.z = 0.0f;

		if (enemy.attackSubState_ == GroundEnemy::GroundAttackState::Melee) {
			enemy.velocity_.x = 0.0f;
			enemy.velocity_.z = 0.0f;
		} else {
			constexpr float kMinKeepDistance = 15.0f;
			const float moveSpeed = kGroundSpeed + (kTrackSpeed - kGroundSpeed) * enemy.trackBlend_;
			const float directionSign = distHorizontalSq < kMinKeepDistance * kMinKeepDistance ? -0.5f : 1.0f;
			if (enemy.trackBlend_ > 0.01f) {
				const Vector3 targetVelocity = { enemy.forward_.x * moveSpeed * directionSign, 0.0f, enemy.forward_.z * moveSpeed * directionSign };
				const float traction = 0.08f + (1.0f - enemy.trackBlend_) * 0.20f;
				enemy.velocity_.x += (targetVelocity.x - enemy.velocity_.x) * traction;
				enemy.velocity_.z += (targetVelocity.z - enemy.velocity_.z) * traction;
			} else if (distHorizontalSq < kMinKeepDistance * kMinKeepDistance) {
				enemy.velocity_.x = -enemy.forward_.x * kGroundSpeed * 0.5f;
				enemy.velocity_.z = -enemy.forward_.z * kGroundSpeed * 0.5f;
			} else {
				enemy.velocity_.x = enemy.forward_.x * kGroundSpeed;
				enemy.velocity_.z = enemy.forward_.z * kGroundSpeed;
			}
		}
	}

	if (!enemy.isGrounded_) {
		enemy.velocity_.x *= 0.98f;
		enemy.velocity_.z *= 0.98f;
	}

	enemy.position_.x += enemy.velocity_.x;
	enemy.position_.z += enemy.velocity_.z;

	const bool isAirborneAttack = enemy.attackSubState_ == GroundEnemy::GroundAttackState::Melee &&
		(enemy.meleePhase_ == GroundEnemy::MeleePhase::Rising || enemy.meleePhase_ == GroundEnemy::MeleePhase::Falling);
	if (!isAirborneAttack) {
		Sphere terrainProbe{};
		terrainProbe.center = enemy.position_;
		terrainProbe.radius = (std::max)(0.5f, (std::max)(enemy.scale_.x, enemy.scale_.z));

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
				if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], enemy.position_.x, enemy.position_.z, terrainY) && terrainY > groundHeight) {
					groundHeight = terrainY;
					foundGroundAtDestination = true;
				}
			}
		}

		if (foundGroundAtDestination) {
			enemy.groundY_ = groundHeight;
			enemy.position_.y = enemy.groundY_ + GetFootOffset(enemy);
			enemy.velocityY_ = 0.0f;
			enemy.isGrounded_ = true;
		}
	}

	UpdateTerrainPose(enemy, obstacles);
}

void GroundEnemyMovementController::UpdateTerrainPose(
	GroundEnemy& enemy,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	Vector3 flatForward = NormalizeOrFallback({ enemy.forward_.x, 0.0f, enemy.forward_.z }, { 0.0f, 0.0f, 1.0f });
	Vector3 right = { flatForward.z, 0.0f, -flatForward.x };
	constexpr float kProbeDistance = 1.0f;
	float front = 0.0f, back = 0.0f, left = 0.0f, rightHeight = 0.0f;
	const bool hasFront = SampleTerrainHeight(obstacles, { enemy.position_.x + flatForward.x * kProbeDistance, enemy.position_.y, enemy.position_.z + flatForward.z * kProbeDistance }, front);
	const bool hasBack = SampleTerrainHeight(obstacles, { enemy.position_.x - flatForward.x * kProbeDistance, enemy.position_.y, enemy.position_.z - flatForward.z * kProbeDistance }, back);
	const bool hasLeft = SampleTerrainHeight(obstacles, { enemy.position_.x - right.x * kProbeDistance, enemy.position_.y, enemy.position_.z - right.z * kProbeDistance }, left);
	const bool hasRight = SampleTerrainHeight(obstacles, { enemy.position_.x + right.x * kProbeDistance, enemy.position_.y, enemy.position_.z + right.z * kProbeDistance }, rightHeight);

	const float pitchTarget = hasFront && hasBack ? std::atan2(front - back, kProbeDistance * 2.0f) : 0.0f;
	const float rollTarget = hasLeft && hasRight ? std::atan2(rightHeight - left, kProbeDistance * 2.0f) : 0.0f;
	enemy.terrainPitch_ += (pitchTarget - enemy.terrainPitch_) * 0.16f;
	enemy.terrainRoll_ += (rollTarget - enemy.terrainRoll_) * 0.16f;

	const bool hasTerrain = hasFront || hasBack || hasLeft || hasRight;
	if (hasTerrain) {
		const Vector3 terrainRotation = { enemy.terrainRoll_, enemy.rotation_.y + kModelForwardYaw, enemy.terrainPitch_ };
		const Matrix4x4 terrainMatrix = MyMath::MakeAffineMatrix(
			{ 1.0f, 1.0f, 1.0f }, terrainRotation, { 0.0f, 0.0f, 0.0f });
		const float footOffset = GetFootOffset(enemy);
		float lowestSupportY = 0.0f;
		for (const Vector3& foot : kFootPositions) {
			const Vector3 rotatedFoot = MyMath::Transform({
				foot.x * enemy.scale_.x,
				-footOffset,
				foot.z * enemy.scale_.z,
			}, terrainMatrix);
			lowestSupportY = (std::min)(lowestSupportY, rotatedFoot.y);
		}
		const float clearanceTarget = std::clamp(-footOffset - lowestSupportY, 0.0f, 0.75f) + 0.025f;
		enemy.terrainClearance_ += (clearanceTarget - enemy.terrainClearance_) * 0.28f;
	} else {
		enemy.terrainClearance_ *= 0.72f;
	}

	for (float& legOffset : enemy.legGroundOffsets_) {
		legOffset += (0.0f - legOffset) * 0.35f;
	}
}

void GroundEnemyMovementController::SnapToGround(
	GroundEnemy& enemy,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	float maxGroundY = -99999.0f;
	bool foundGround = false;

	for (const auto& obs : obstacles) {
		if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled()) continue;

		if (obs->IsUseMeshCollider()) {
			const auto& triangles = obs->GetWorldTriangles();
			Sphere terrainSample{};
			terrainSample.center = enemy.position_;
			terrainSample.radius = 0.25f;
			for (const size_t triangleIndex : obs->GetNearbyWorldTriangleIndices(terrainSample)) {
				const Triangle& tri = triangles[triangleIndex];
				if (std::abs(tri.normal.y) > 0.1f) {
					float triY = 0.0f;
					if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], enemy.position_.x, enemy.position_.z, triY)) {
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
			float sampleX = enemy.position_.x + offsets[i][0];
			float sampleZ = enemy.position_.z + offsets[i][1];
			for (const auto& obs : obstacles) {
				if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled()) continue;
				if (obs->IsUseMeshCollider()) {
					const auto& triangles = obs->GetWorldTriangles();
					Sphere terrainSample{};
					terrainSample.center = { sampleX, enemy.position_.y, sampleZ };
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
				if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], enemy.position_.x, enemy.position_.z, triY) && triY > maxGroundY) {
					maxGroundY = triY;
					foundGround = true;
				}
			}
		}
	}

	const float footOffset = GetFootOffset(enemy);
	if (foundGround) {
		enemy.position_.y = maxGroundY + footOffset;
	} else {
		enemy.position_.y = (std::max)(enemy.position_.y, footOffset);
	}

	enemy.velocityY_ = 0.0f;
	enemy.isGrounded_ = true;
	enemy.UpdateModel();
}

float GroundEnemyMovementController::GetFootOffset(const GroundEnemy& enemy) const {
	if (!enemy.object_ || !enemy.object_->GetModel()) {
		return enemy.scale_.y;
	}

	const Vector3 center = enemy.object_->GetModel()->GetBoundsCenter();
	const Vector3 halfExtents = enemy.object_->GetModel()->GetHalfExtents();
	float localBottomY = center.y - halfExtents.y;

	for (size_t i = 0; i < enemy.legs_.size(); ++i) {
		if (!enemy.legs_[i] || !enemy.legs_[i]->GetModel()) {
			continue;
		}
		const Vector3 legCenter = enemy.legs_[i]->GetModel()->GetBoundsCenter();
		const Vector3 legHalfExtents = enemy.legs_[i]->GetModel()->GetHalfExtents();
		localBottomY = (std::min)(localBottomY, kLegPivots[i].y + legCenter.y - legHalfExtents.y);
	}
	return (std::max)(0.0f, -localBottomY * std::abs(enemy.scale_.y)) + 0.025f;
}

void GroundEnemyMovementController::UpdateFlightPathMovement(GroundEnemy& enemy) const {
	if (enemy.flightPathPoints_.size() < 2) return;

	const size_t segmentCount = enemy.flightPathLoop_ ? enemy.flightPathPoints_.size() : enemy.flightPathPoints_.size() - 1;
	if (segmentCount == 0) return;

	Vector3 oldPosition = enemy.position_;
	float remainingDistance = enemy.flightPathSpeed_;

	while (remainingDistance > 0.0f) {
		size_t nextIndex = ResolvePathIndex(static_cast<int>(enemy.flightPathSegmentIndex_ + 1), enemy.flightPathPoints_.size(), enemy.flightPathLoop_);
		Vector3 segmentVector = {
			enemy.flightPathPoints_[nextIndex].x - enemy.flightPathPoints_[enemy.flightPathSegmentIndex_].x,
			0.0f,
			enemy.flightPathPoints_[nextIndex].z - enemy.flightPathPoints_[enemy.flightPathSegmentIndex_].z
		};
		float segmentLength = std::sqrt(segmentVector.x * segmentVector.x + segmentVector.z * segmentVector.z);

		if (segmentLength < 0.0001f) {
			++enemy.flightPathSegmentIndex_;
			enemy.flightPathSegmentT_ = 0.0f;
			if (enemy.flightPathSegmentIndex_ >= segmentCount) {
				if (enemy.flightPathLoop_) {
					enemy.flightPathSegmentIndex_ = 0;
				} else {
					enemy.flightPathSegmentIndex_ = segmentCount - 1;
					enemy.flightPathSegmentT_ = 1.0f;
					break;
				}
			}
			continue;
		}

		float distanceToSegmentEnd = segmentLength * (1.0f - enemy.flightPathSegmentT_);
		if (remainingDistance >= distanceToSegmentEnd) {
			remainingDistance -= distanceToSegmentEnd;
			enemy.flightPathSegmentT_ = 0.0f;
			++enemy.flightPathSegmentIndex_;
			if (enemy.flightPathSegmentIndex_ >= segmentCount) {
				if (enemy.flightPathLoop_) {
					enemy.flightPathSegmentIndex_ = 0;
				} else {
					enemy.flightPathSegmentIndex_ = segmentCount - 1;
					enemy.flightPathSegmentT_ = 1.0f;
					break;
				}
			}
		} else {
			enemy.flightPathSegmentT_ += remainingDistance / segmentLength;
			remainingDistance = 0.0f;
		}
	}

	Vector3 targetPos = EvaluateFlightPath(enemy.flightPathPoints_, enemy.flightPathSegmentIndex_, enemy.flightPathSegmentT_, enemy.flightPathLoop_);
	enemy.position_.x = targetPos.x;
	enemy.position_.z = targetPos.z;

	Vector3 diff = { enemy.position_.x - oldPosition.x, 0.0f, enemy.position_.z - oldPosition.z };
	float diffLen = std::sqrt(diff.x * diff.x + diff.z * diff.z);
	if (diffLen > 0.001f) {
		enemy.forward_ = { diff.x / diffLen, 0.0f, diff.z / diffLen };
		enemy.rotation_.y = std::atan2(enemy.forward_.x, enemy.forward_.z);
	}

	enemy.velocity_.x = 0.0f;
	enemy.velocity_.z = 0.0f;
}
