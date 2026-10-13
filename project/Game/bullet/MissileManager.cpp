#include "MissileManager.h"
#include "Game/enemy/Enemy.h"
#include "Game/obstacle/Obstacle.h" // 追加
#include <cmath>

namespace {
    bool GetTriangleY(const Vector3& p0, const Vector3& p1, const Vector3& p2, float x, float z, float& outY) {
        const float denom = (p1.z - p2.z) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.z - p2.z);
        if (std::abs(denom) < 0.00001f) {
            return false;
        }

        const float w0 = ((p1.z - p2.z) * (x - p2.x) + (p2.x - p1.x) * (z - p2.z)) / denom;
        const float w1 = ((p2.z - p0.z) * (x - p2.x) + (p0.x - p2.x) * (z - p2.z)) / denom;
        const float w2 = 1.0f - w0 - w1;
        if (w0 < -0.01f || w1 < -0.01f || w2 < -0.01f) {
            return false;
        }

        outY = w0 * p0.y + w1 * p1.y + w2 * p2.y;
        return true;
    }
}

void MissileManager::Initialize(ParticleManager* particleManager) {
	particleManager_ = particleManager;
	missiles_.clear();
}

void MissileManager::Update(Camera *camera, std::list<std::unique_ptr<Enemy>> &enemies, const std::list<std::unique_ptr<Obstacle>> &obstacles, std::vector<Vector3> &hitPositions, std::vector<Vector3> &destroyedPositions) {
    for (auto it = missiles_.begin(); it != missiles_.end(); ) {
        Missile *missile = it->get();

        const Vector3 previousPos = missile->GetPosition();

        // ==========================================
        // 2. ミサイルの更新（自身が保持するターゲットに向かう）
        // ==========================================
        missile->Update(camera);

        // ==========================================
        // 3. 移動後の最新座標で当たり判定
        // ==========================================
        Vector3 mPos = missile->GetPosition();

        // 3-1. 障害物との当たり判定
        bool hitObstacle = false;
        Sphere bulletSphere;
        bulletSphere.center = mPos;
        bulletSphere.radius = missile->GetCollisionRadius();

        for (const auto& obstacle : obstacles) {
            if (!obstacle || obstacle->IsStageBounds() || !obstacle->IsCollisionEnabled()) {
                continue;
            }

            OBB obsOBB = obstacle->GetOBB(); if (!MyMath::IsCollision(bulletSphere, obsOBB)) { continue; } if (obstacle->IsUseMeshCollider()) {
                const std::vector<Triangle>& triangles = obstacle->GetWorldTriangles();
                Vector3 pushVector;
                for (const size_t triangleIndex : obstacle->GetNearbyWorldTriangleIndices(bulletSphere)) {
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

                    // 地形を1フレームでまたいでも、移動後の弾が地表より下なら確実に命中扱いにする。
                    // OBJの面方向には依存せず、弾の下面と直下の地表高度を比較する。
                    if (std::abs(tri.normal.y) > 0.1f) {
                        float terrainY = 0.0f;
                        const bool endInsideXZ =
                            mPos.x >= minX - 0.01f && mPos.x <= maxX + 0.01f &&
                            mPos.z >= minZ - 0.01f && mPos.z <= maxZ + 0.01f;
                        bool touchedGround = false;
                        if (endInsideXZ &&
                            GetTriangleY(tri.p[0], tri.p[1], tri.p[2], mPos.x, mPos.z, terrainY) &&
                            mPos.y - bulletSphere.radius <= terrainY) {
                            touchedGround = true;
                        }

                        // 高速移動で終点が別の三角形へ出る場合も、軌跡中央で地表を横切っていないか確認する。
                        const Vector3 midPos = {
                            (previousPos.x + mPos.x) * 0.5f,
                            (previousPos.y + mPos.y) * 0.5f,
                            (previousPos.z + mPos.z) * 0.5f
                        };
                        const bool midInsideXZ =
                            midPos.x >= minX - 0.01f && midPos.x <= maxX + 0.01f &&
                            midPos.z >= minZ - 0.01f && midPos.z <= maxZ + 0.01f;
                        if (!touchedGround && midInsideXZ &&
                            GetTriangleY(tri.p[0], tri.p[1], tri.p[2], midPos.x, midPos.z, terrainY) &&
                            midPos.y - bulletSphere.radius <= terrainY &&
                            previousPos.y + bulletSphere.radius >= terrainY) {
                            touchedGround = true;
                        }

                        if (touchedGround) {
                            missile->OnCollision();
                            hitObstacle = true;
                            break;
                        }

                        // 地形面は上向きでも弾着として扱う。
                        continue;
                    }

                    if (bulletSphere.center.x + bulletSphere.radius < minX || bulletSphere.center.x - bulletSphere.radius > maxX ||
                        bulletSphere.center.y + bulletSphere.radius < minY || bulletSphere.center.y - bulletSphere.radius > maxY ||
                        bulletSphere.center.z + bulletSphere.radius < minZ || bulletSphere.center.z - bulletSphere.radius > maxZ) {
                        continue;
                    }

                    if (MyMath::IsCollision(bulletSphere, tri, pushVector)) {
                        missile->OnCollision();
                        hitObstacle = true;
                        break;
                    }
                }
                if (hitObstacle) {
                    break;
                }
            } else {
                OBB obsOBB = obstacle->GetOBB();
                if (MyMath::IsCollision(bulletSphere, obsOBB)) {
                    missile->OnCollision();
                    hitObstacle = true;
                    break;
                }
            }
        }

		if (hitObstacle) {
			hitPositions.push_back(missile->GetPosition());
		}

        if (!hitObstacle) {
            for (auto &enemy : enemies) {
                if (!enemy->IsDead() && !missile->IsDead()) {
                    OBB enemyOBB = enemy->GetOBB();
                    if (MyMath::IsCollision(bulletSphere, enemyOBB)) {
                        missile->OnCollision();
                        enemy->TakeDamage(1);

                        if (enemy->IsDead()) {
                            destroyedPositions.push_back(enemy->GetPosition());
                        } else {
                            hitPositions.push_back(enemy->GetPosition());
                        }
                    }
                }
            }
        }

        // ==========================================
        // 4. 寿命・衝突で死んでいたら削除
        // ==========================================
        if (missile->IsDead()) {
            it = missiles_.erase(it);
        } else {
            ++it;
        }
    }
}

void MissileManager::UpdateModels(Camera *camera) {
    for (const auto &missile : missiles_) {
        missile->UpdateModel(camera);
    }
}

void MissileManager::Draw() {
	for (const auto &missile : missiles_) {
		missile->Draw();
	}
}

void MissileManager::Shoot(const Vector3 &position, const Vector3 &velocity, MissileType type, const MissileTuning &tuning, Enemy* target) {
	// 新しいミサイルを生み出してリストに追加する
	auto newMissile = std::make_unique<Missile>();
	newMissile->Initialize(position, velocity, type, tuning, particleManager_, target);

	missiles_.push_back(std::move(newMissile));
}

void MissileManager::ClearTarget(const Enemy* target) {
    if (!target) {
        return;
    }

    for (const auto& missile : missiles_) {
        if (missile) {
            missile->ClearTargetIfMatches(target);
        }
    }
}

void MissileManager::ClearAllTargets() {
    for (const auto& missile : missiles_) {
        if (missile) {
            missile->ClearTargetIfMatches(missile->GetTarget());
        }
    }
}

