#include "PlayerCollisionController.h"

#include "Player.h"
#include "Game/obstacle/Obstacle.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
	Vector3 Add(const Vector3& a, const Vector3& b) {
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	Vector3 Scale(const Vector3& value, float scalar) {
		return { value.x * scalar, value.y * scalar, value.z * scalar };
	}

	float Dot(const Vector3& a, const Vector3& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	float Abs(float value) {
		return value < 0.0f ? -value : value;
	}

	Vector3 AbsVector(const Vector3& value) {
		return { Abs(value.x), Abs(value.y), Abs(value.z) };
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

	float ProjectionRadius(const OBB& obb, const Vector3& axis) {
		return obb.size.x * Abs(Dot(obb.orientations[0], axis)) +
			obb.size.y * Abs(Dot(obb.orientations[1], axis)) +
			obb.size.z * Abs(Dot(obb.orientations[2], axis));
	}

	float GetAxisSize(const Vector3& value, int axisIndex) {
		if (axisIndex == 0) return value.x;
		if (axisIndex == 1) return value.y;
		return value.z;
	}

	Vector3 ComposeFromAxes(const OBB& basis, const Vector3& amount) {
		return Add(Add(Scale(basis.orientations[0], amount.x), Scale(basis.orientations[1], amount.y)), Scale(basis.orientations[2], amount.z));
	}

	void CancelVerticalVelocityIntoPush(Vector3& velocity, const Vector3& pushOut) {
		if ((pushOut.y > 0.0f && velocity.y < 0.0f) || (pushOut.y < 0.0f && velocity.y > 0.0f)) {
			velocity.y = 0.0f;
		}
	}
}

Vector3 PlayerCollisionController::GetWorldHalfExtents(const Player& player) const {
	if (!player.object_ || !player.object_->GetModel()) {
		return { 0.2f, 0.2f, 0.2f };
	}
	if (player.IsTransformPlayerModel()) {
		if (player.currentMode_ == PlayerMode::Fighter) return { 0.72f, 0.20f, 0.88f };
		if (player.currentMode_ == PlayerMode::Gerwalk) return { 0.58f, 0.65f, 0.52f };
		return { 0.42f, 1.12f, 0.34f };
	}

	if (player.modelName_.find("vf-15c") != std::string::npos) {
		const float scale = player.modelScale_.x; // 0.08f
		if (player.currentMode_ == PlayerMode::Fighter) {
			return { 2.8f * scale, 0.8f * scale, 3.0f * scale }; // { 0.224f, 0.064f, 0.24f }
		} else if (player.currentMode_ == PlayerMode::Gerwalk) {
			return { 2.8f * scale, 2.8f * scale, 2.8f * scale }; // { 0.224f, 0.224f, 0.224f }
		} else {
			return { 2.2f * scale, 4.5f * scale, 2.2f * scale }; // { 0.176f, 0.36f, 0.176f }
		}
	}

	const Vector3 localHalf = player.object_->GetModel()->GetHalfExtents();
	const Vector3 scale = AbsVector(player.object_->GetScale());
	return { localHalf.x * scale.x, localHalf.y * scale.y, localHalf.z * scale.z };
}

float PlayerCollisionController::GetCollisionRadius(const Player& player) const {
	const Vector3 halfExtents = GetWorldHalfExtents(player);
	float radius = halfExtents.x;
	if (halfExtents.y > radius) radius = halfExtents.y;
	if (halfExtents.z > radius) radius = halfExtents.z;
	return radius;
}

OBB PlayerCollisionController::GetOBB(const Player& player) const {
	OBB obb{};
	obb.center = player.position_;
	obb.size = GetWorldHalfExtents(player);

	Matrix4x4 rotationMatrix = MyMath::MakeRotateMatrix(player.quaternion_);
	obb.orientations[0] = MyMath::Normalize(Vector3{ rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2] });
	obb.orientations[1] = MyMath::Normalize(Vector3{ rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2] });
	obb.orientations[2] = MyMath::Normalize(Vector3{ rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2] });

	if (player.object_ && player.object_->GetModel()) {
		Vector3 localCenter = player.object_->GetModel()->GetBoundsCenter();

		// vf-15cモデルの場合はアニメーションによる変形を考慮して、形態ごとの適切なオフセットを設定する
		if (player.modelName_.find("vf-15c") != std::string::npos) {
			if (player.currentMode_ == PlayerMode::Fighter) {
				localCenter.y -= 0.0f;
			} else if (player.currentMode_ == PlayerMode::Gerwalk) {
				localCenter.y -= 0.8f; // 中心を下げて脚部をカバーする
			} else {
				localCenter.y -= 1.2f; // 中心を下げて人型の長身をカバーする
			}
		}

		const Vector3 scale = player.object_->GetScale();
		const Vector3 scaledCenter = { localCenter.x * scale.x, localCenter.y * scale.y, localCenter.z * scale.z };
		const Vector3 rotatedCenter = MyMath::Transform(scaledCenter, rotationMatrix);
		obb.center = Add(player.position_, rotatedCenter);
	}

	return obb;
}

void PlayerCollisionController::ResolveCollisions(
	Player& player,
	const std::list<std::unique_ptr<Obstacle>>& obstacles) const {
	OBB playerOBB = GetOBB(player);

	for (const auto& obstacle : obstacles) {
		if (!obstacle || !obstacle->IsCollisionEnabled()) {
			continue;
		}

		if (obstacle->IsUseMeshCollider() && !obstacle->IsStageBounds()) {
			const std::vector<Triangle>& triangles = obstacle->GetWorldTriangles();

			std::vector<Sphere> playerSpheres;
			const float scale = player.modelScale_.x; // 0.08f

			if (player.modelName_.find("vf-15c") != std::string::npos) {
				if (player.currentMode_ == PlayerMode::Fighter) {
					Sphere s;
					s.center = playerOBB.center;
					s.radius = 1.2f * scale;
					playerSpheres.push_back(s);
				} else if (player.currentMode_ == PlayerMode::Gerwalk) {
					// ガウォーク形態: 2つの球を縦に並べる（上部と足元）
					float r = 2.0f * scale;
					Sphere sTop;
					sTop.center = playerOBB.center;
					sTop.center.y += 1.2f * scale;
					sTop.radius = r;
					playerSpheres.push_back(sTop);

					Sphere sBottom;
					sBottom.center = playerOBB.center;
					sBottom.center.y -= 0.8f * scale;
					sBottom.radius = r;
					playerSpheres.push_back(sBottom);
				} else {
					// バトロイド形態（人型）: 3つの球を縦に並べてカプセルを表現（頭/胸、腰/腿、足元）
					float r = 2.2f * scale;
					Sphere sTop;
					sTop.center = playerOBB.center;
					sTop.center.y += 1.5f * scale;
					sTop.radius = r;
					playerSpheres.push_back(sTop);

					Sphere sMid;
					sMid.center = playerOBB.center;
					sMid.center.y -= 0.5f * scale;
					sMid.radius = r;
					playerSpheres.push_back(sMid);

					Sphere sBottom;
					sBottom.center = playerOBB.center;
					sBottom.center.y -= 2.5f * scale;
					sBottom.radius = r;
					playerSpheres.push_back(sBottom);
				}
			} else {
				Sphere s;
				s.center = playerOBB.center;
				s.radius = GetCollisionRadius(player);
				playerSpheres.push_back(s);
			}

			// 地底抜け・埋まり防止：足元の地形の最高標高を特定し、潜り込んでいたら足元が地上にピッタリ乗る高さに即時補正する
			float maxTerrainY = -99999.0f;
			bool foundTerrain = false;
			Sphere terrainSample{};
			terrainSample.center = player.position_;
			terrainSample.radius = 0.25f;
			for (const size_t triangleIndex : obstacle->GetNearbyWorldTriangleIndices(terrainSample)) {
				const Triangle& tri = triangles[triangleIndex];
				if (std::abs(tri.normal.y) > 0.15f) {
					float triY = 0.0f;
					if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], player.position_.x, player.position_.z, triY)) {
						if (triY > maxTerrainY) {
							maxTerrainY = triY;
							foundTerrain = true;
						}
					}
				}
			}

			const Vector3 halfExtents = GetWorldHalfExtents(player);
			const float minRequiredY = maxTerrainY + halfExtents.y;

			if (foundTerrain && player.position_.y < minRequiredY) {
				const float lift = minRequiredY - player.position_.y;
				player.position_.y = minRequiredY;
				if (player.velocity_.y < 0.0f) {
					player.velocity_.y = 0.0f;
				}
				// 高さ補正後も判定球を旧位置に残さず、次の三角形判定で
				// 横方向へ押し戻される一瞬の停止を防ぐ。
				for (Sphere& playerSphere : playerSpheres) {
					playerSphere.center.y += lift;
				}
				playerOBB = GetOBB(player);
			}

			for (auto& playerSphere : playerSpheres) {
				for (const size_t triangleIndex : obstacle->GetNearbyWorldTriangleIndices(playerSphere)) {
					const Triangle& tri = triangles[triangleIndex];
					float minX = tri.p[0].x;
					if (tri.p[1].x < minX) minX = tri.p[1].x;
					if (tri.p[2].x < minX) minX = tri.p[2].x;

					float maxX = tri.p[0].x;
					if (tri.p[1].x > maxX) maxX = tri.p[1].x;
					if (tri.p[2].x > maxX) maxX = tri.p[2].x;

					float minY = tri.p[0].y;
					if (tri.p[1].y < minY) minY = tri.p[1].y;
					if (tri.p[2].y < minY) minY = tri.p[2].y;

					float maxY = tri.p[0].y;
					if (tri.p[1].y > maxY) maxY = tri.p[1].y;
					if (tri.p[2].y > maxY) maxY = tri.p[2].y;

					float minZ = tri.p[0].z;
					if (tri.p[1].z < minZ) minZ = tri.p[1].z;
					if (tri.p[2].z < minZ) minZ = tri.p[2].z;

					float maxZ = tri.p[0].z;
					if (tri.p[1].z > maxZ) maxZ = tri.p[1].z;
					if (tri.p[2].z > maxZ) maxZ = tri.p[2].z;

					if (playerSphere.center.x + playerSphere.radius < minX || playerSphere.center.x - playerSphere.radius > maxX ||
						playerSphere.center.y + playerSphere.radius < minY || playerSphere.center.y - playerSphere.radius > maxY ||
						playerSphere.center.z + playerSphere.radius < minZ || playerSphere.center.z - playerSphere.radius > maxZ) {
						continue;
					}

					Vector3 pushVector;
					if (MyMath::IsCollision(playerSphere, tri, pushVector)) {
						// 地面（上向きの面）との衝突で、裏側に回り込んで下に押し下げられるのを防止
						if (std::abs(tri.normal.y) > 0.3f) {
							float pushLen = MyMath::Length(pushVector);
							if (pushLen < 0.05f) pushLen = 0.05f;
							Vector3 groundNormal = tri.normal;
							if (groundNormal.y < 0.0f) {
								groundNormal = MyMath::Multiply(-1.0f, groundNormal);
							}
							// 低空ファイターは地形の斜面法線で横速度まで失わないよう、
							// なだらかな地面では真上にだけ押し戻す。
							if (player.currentMode_ == PlayerMode::Fighter && groundNormal.y >= 0.65f) {
								pushVector = { 0.0f, pushLen, 0.0f };
							} else {
								pushVector = MyMath::Multiply(pushLen, groundNormal);
							}
						}

						player.position_.x += pushVector.x;
						player.position_.y += pushVector.y;
						player.position_.z += pushVector.z;
						CancelVerticalVelocityIntoPush(player.velocity_, pushVector);

						for (auto& otherSphere : playerSpheres) {
							otherSphere.center.x += pushVector.x;
							otherSphere.center.y += pushVector.y;
							otherSphere.center.z += pushVector.z;
						}

						playerOBB.center.x += pushVector.x;
						playerOBB.center.y += pushVector.y;
						playerOBB.center.z += pushVector.z;

						Vector3 pushNormal = MyMath::Normalize(pushVector);
						float dot = MyMath::Dot(player.velocity_, pushNormal);
						if (dot < 0.0f) {
							player.velocity_.x -= dot * pushNormal.x;
							player.velocity_.y -= dot * pushNormal.y;
							player.velocity_.z -= dot * pushNormal.z;
						}
					}
				}
			}
			if (foundTerrain && player.position_.y < minRequiredY) {
				player.position_.y = minRequiredY;
				if (player.velocity_.y < 0.0f) {
					player.velocity_.y = 0.0f;
				}
			}
			continue;
		}

		const OBB obsOBB = obstacle->GetOBB();
		const Vector3 diff = {
			playerOBB.center.x - obsOBB.center.x,
			playerOBB.center.y - obsOBB.center.y,
			playerOBB.center.z - obsOBB.center.z,
		};
		const float localDistance[3] = {
			Dot(diff, obsOBB.orientations[0]),
			Dot(diff, obsOBB.orientations[1]),
			Dot(diff, obsOBB.orientations[2]),
		};
		const float playerProjection[3] = {
			ProjectionRadius(playerOBB, obsOBB.orientations[0]),
			ProjectionRadius(playerOBB, obsOBB.orientations[1]),
			ProjectionRadius(playerOBB, obsOBB.orientations[2]),
		};

		if (obstacle->IsStageBounds()) {
			Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

			const float warningThreshold = player.boundaryWarningThreshold_;
			float minDistanceToSideWall = 99999.0f;
			Vector3 closestSideWallNormal = { 0.0f, 0.0f, 1.0f };
			float distanceToCeiling = 99999.0f;
			Vector3 ceilingNormal = { 0.0f, 1.0f, 0.0f };

			for (int axis = 0; axis < 3; ++axis) {
				const float limit = (std::max)(0.0f, GetAxisSize(obsOBB.size, axis) - playerProjection[axis]);

				// 警告判定用の距離計算
				if (axis != 1) {
					float distPositive = limit - localDistance[axis];
					if (distPositive < minDistanceToSideWall) {
						minDistanceToSideWall = distPositive;
						closestSideWallNormal = obsOBB.orientations[axis];
					}

					float distNegative = limit + localDistance[axis];
					if (distNegative < minDistanceToSideWall) {
						minDistanceToSideWall = distNegative;
						closestSideWallNormal = { -obsOBB.orientations[axis].x, -obsOBB.orientations[axis].y, -obsOBB.orientations[axis].z };
					}
				} else {
					// Ceiling only. The negative Y side is the floor, so keep it out of the warning.
					float distCeiling = limit - localDistance[axis];
					if (distCeiling < distanceToCeiling) {
						distanceToCeiling = distCeiling;
						ceilingNormal = obsOBB.orientations[axis];
					}
				}

				if (localDistance[axis] > limit) {
					if (axis == 0) localPushOut.x = limit - localDistance[axis];
					if (axis == 1) localPushOut.y = limit - localDistance[axis];
					if (axis == 2) localPushOut.z = limit - localDistance[axis];
				} else if (localDistance[axis] < -limit) {
					if (axis == 0) localPushOut.x = -limit - localDistance[axis];
					// axis == 1 のマイナスY方向（底面）は山モデルがあるので、山への着地を妨げないよう押し戻しを行わない
					if (axis == 2) localPushOut.z = -limit - localDistance[axis];
				}
			}

			auto registerBoundaryAlert = [&](float intensity, const Vector3& position, const Vector3& normal) {
				if (intensity > player.boundaryWarningIntensity_) {
					player.isNearBoundary_ = true;
					player.boundaryWarningIntensity_ = intensity;
					player.boundaryAlertPosition_ = position;
					player.boundaryAlertNormal_ = normal;
				}
			};

			Vector3 ceilingPosition = {
				playerOBB.center.x + ceilingNormal.x * distanceToCeiling,
				playerOBB.center.y + ceilingNormal.y * distanceToCeiling,
				playerOBB.center.z + ceilingNormal.z * distanceToCeiling
			};

			// 警告フラグの更新
			if (minDistanceToSideWall < warningThreshold) {
				player.isNearWallBoundary_ = true;
				player.wallBoundaryWarningIntensity_ = 1.0f - (std::max)(0.0f, minDistanceToSideWall) / warningThreshold;
				player.wallBoundaryAlertNormal_ = closestSideWallNormal;

				player.wallBoundaryAlertPosition_ = {
					playerOBB.center.x + closestSideWallNormal.x * minDistanceToSideWall,
					playerOBB.center.y + closestSideWallNormal.y * minDistanceToSideWall,
					playerOBB.center.z + closestSideWallNormal.z * minDistanceToSideWall
				};

				if (distanceToCeiling < warningThreshold) {
					constexpr float kBoundaryAlertHalfHeight = 2.0f;
					player.wallBoundaryAlertPosition_.y = (std::min)(player.wallBoundaryAlertPosition_.y, ceilingPosition.y - kBoundaryAlertHalfHeight);
				}

				registerBoundaryAlert(player.wallBoundaryWarningIntensity_, player.wallBoundaryAlertPosition_, player.wallBoundaryAlertNormal_);
			}

			if (distanceToCeiling < warningThreshold) {
				player.isNearCeilingBoundary_ = true;
				player.ceilingBoundaryWarningIntensity_ = 1.0f - (std::max)(0.0f, distanceToCeiling) / warningThreshold;
				player.ceilingBoundaryAlertNormal_ = ceilingNormal;
				player.ceilingBoundaryAlertPosition_ = ceilingPosition;

				registerBoundaryAlert(player.ceilingBoundaryWarningIntensity_, player.ceilingBoundaryAlertPosition_, player.ceilingBoundaryAlertNormal_);
			}

			if (localPushOut.x != 0.0f || localPushOut.y != 0.0f || localPushOut.z != 0.0f) {
				Vector3 worldPushOut = ComposeFromAxes(obsOBB, localPushOut);
				player.position_.x += worldPushOut.x;
				player.position_.y += worldPushOut.y;
				player.position_.z += worldPushOut.z;
				CancelVerticalVelocityIntoPush(player.velocity_, worldPushOut);
				playerOBB.center = Add(playerOBB.center, worldPushOut);
			}
		} else {
			if (!MyMath::IsCollision(playerOBB, obsOBB)) {
				continue;
			}

			const float overlapX = obsOBB.size.x + playerProjection[0] - std::abs(localDistance[0]);
			const float overlapY = obsOBB.size.y + playerProjection[1] - std::abs(localDistance[1]);
			const float overlapZ = obsOBB.size.z + playerProjection[2] - std::abs(localDistance[2]);

			if (overlapX > 0.0f && overlapY > 0.0f && overlapZ > 0.0f) {
				Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

				if (overlapX < overlapY && overlapX < overlapZ) {
					localPushOut.x = (localDistance[0] > 0.0f) ? overlapX : -overlapX;
				} else if (overlapY < overlapX && overlapY < overlapZ) {
					localPushOut.y = (localDistance[1] > 0.0f) ? overlapY : -overlapY;
				} else {
					localPushOut.z = (localDistance[2] > 0.0f) ? overlapZ : -overlapZ;
				}

				Vector3 worldPushOut = ComposeFromAxes(obsOBB, localPushOut);
				player.position_.x += worldPushOut.x;
				player.position_.y += worldPushOut.y;
				player.position_.z += worldPushOut.z;
				CancelVerticalVelocityIntoPush(player.velocity_, worldPushOut);
				playerOBB.center = Add(playerOBB.center, worldPushOut);
			}
		}
	}
}
