#include "GroundEnemy.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"
#include "engine/Math/MyMath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    constexpr float kGroundSpeed = 0.08f;
    constexpr float kHighJumpPower = 0.38f;
    constexpr int kGroundEnemyMaxHp = 3;

    Vector3 NormalizeOrFallback(const Vector3 &v, const Vector3 &fallback) {
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

    Vector3 CatmullRom(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        Vector3 result = {
            0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
            0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
            0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t + (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 + (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3)
        };
        return result;
    }

    Vector3 EvaluateFlightPath(const std::vector<Vector3> &points, size_t segmentIndex, float t, bool loop) {
        int baseIndex = static_cast<int>(segmentIndex);
        const Vector3 &p0 = points[ResolvePathIndex(baseIndex - 1, points.size(), loop)];
        const Vector3 &p1 = points[ResolvePathIndex(baseIndex, points.size(), loop)];
        const Vector3 &p2 = points[ResolvePathIndex(baseIndex + 1, points.size(), loop)];
        const Vector3 &p3 = points[ResolvePathIndex(baseIndex + 2, points.size(), loop)];
        return CatmullRom(p0, p1, p2, p3, t);
    }
}

void GroundEnemy::Initialize(const Vector3 &position) {
    if (!object_) {
        object_ = std::make_unique<Object3d>();
        object_->Initialize(Object3dCommon::GetInstance());
    }

    ModelManager* modelMgr = ModelManager::GetInstance();
    if (modelMgr && modelMgr->FindModel("vf-15c/scene.gltf")) {
        object_->SetModel("vf-15c/scene.gltf");

        std::string dir = "resources/vf-15c";
        std::string file = "scene.gltf";
        animationData_ = LoadAnimationFile(dir, file);
        Node rootNode = object_->GetModel()->GetModelData().rootNode;
        skeleton_ = CreateSkeleton(rootNode);
        if (!skeleton_.joints.empty()) {
            enableSkinning_ = true;
            skinCluster_ = object_->GetModel()->CreateSkinCluster(skeleton_);
        }
    } else {
        object_->SetModel("EnemyBox");
    }

    if (object_->GetModel()) {
        // 地上敵(VF3)らしいグリーン系のメカカラー
        object_->GetModel()->SetColor({ 0.2f, 0.65f, 0.35f, 1.0f });
    }

    scale_ = { 0.12f, 0.12f, 0.12f };
    object_->SetScale(scale_);

    position_ = position;
    // モデルの中心ではなく底面（足元）が地面の上に立つようにY座標を調整
    position_.y = position.y + scale_.y;
    groundY_ = position.y;
    rotation_ = { 0.0f, 0.0f, 0.0f };
    forward_ = { 0.0f, 0.0f, 1.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };
    velocityY_ = 0.0f;
    isGrounded_ = true;
    jumpTimer_ = 0;

    isDead_ = false;
    hp_ = kGroundEnemyMaxHp;
    // 地形への接地は維持したまま、ロックオンされていなくても自律的に追跡・攻撃する。
    isChasingPlayer_ = true;

    // 近接攻撃判定用ブロックの初期化
    meleeBox_ = std::make_unique<Object3d>();
    meleeBox_->Initialize(Object3dCommon::GetInstance());
    meleeBox_->SetModel("EnemyBox");
    if (meleeBox_->GetModel()) {
        // 赤く光る攻撃ブロック
        meleeBox_->GetModel()->SetColor({ 1.0f, 0.15f, 0.15f, 0.85f });
    }
    meleeBox_->SetScale(meleeBoxScale_);
    meleeBoxPos_ = { -9999.0f, -9999.0f, -9999.0f };
    meleeBox_->SetTranslate(meleeBoxPos_);
    meleeBox_->Update();
    isMeleeActive_ = false;
    meleeTimer_ = 0;
    attackSubState_ = GroundAttackState::Idle;

    object_->SetTranslate(position_);
    object_->SetRotate(rotation_);
    object_->Update();
}

void GroundEnemy::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    if (isDead_) return;

    // 1. 地上移動とハイジャンプ・重力物理
    UpdateGroundMovement(playerPos, obstacles);

    // 2. 攻撃AI（ミサイル、ガトリング、近接攻撃）
    if (isChasingPlayer_) {
        UpdateAttackAI(playerPos, bulletManager);
    }

    // 3. 当たり判定（障害物押し出し）
    // UpdateGroundMovement() 内で地形への接地まで済ませているため、ここで
    // 全く同じ地形検索をもう一度行わない。
    CheckCollision(obstacles);

    // 4. モデルの更新
    UpdateModel();
}

void GroundEnemy::UpdateGroundMovement(const Vector3 &playerPos, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    // 重力計算（落下）
    velocityY_ -= gravity_;
    position_.y += velocityY_;

    // プレイヤーと完全に同等のメッシュコライダー（球 vs メッシュ三角形交差・押し出し）当たり判定
    Sphere enemySphere;
    enemySphere.center = position_;
    enemySphere.radius = scale_.y;

    bool hitGroundThisFrame = false;

    for (const auto &obs : obstacles) {
        if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled()) continue;

        if (obs->IsUseMeshCollider()) {
            const auto& triangles = obs->GetWorldTriangles();
            // XZ の検索範囲は接地判定と球メッシュ判定で共通。空間グリッドの
            // 候補を一度だけ取り出して使い回し、地上敵が増えた際のCPU負荷を抑える。
            const auto& nearbyTriangleIndices = obs->GetNearbyWorldTriangleIndices(enemySphere);

            // 地底抜け防止＆埋まり防止：足元の地形の最高標高を特定し、身体が埋まらない高さ(footOffset)に保つ
            float maxTerrainY = -99999.0f;
            bool foundTerrain = false;
            for (const size_t triangleIndex : nearbyTriangleIndices) {
                const Triangle& tri = triangles[triangleIndex];
                if (std::abs(tri.normal.y) > 0.2f) {
                    float triY = 0.0f;
                    if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], position_.x, position_.z, triY)) {
                        if (triY > maxTerrainY) {
                            maxTerrainY = triY;
                            foundTerrain = true;
                        }
                    }
                }
            }

            const float footOffset = GetFootOffset();
            if (foundTerrain && position_.y < maxTerrainY + footOffset) {
                position_.y = maxTerrainY + footOffset;
                if (velocityY_ < 0.0f) {
                    velocityY_ = 0.0f;
                }
                enemySphere.center = position_;
                hitGroundThisFrame = true;
            }

            for (const size_t triangleIndex : nearbyTriangleIndices) {
                const Triangle& tri = triangles[triangleIndex];
                Vector3 pushVector;
                if (MyMath::IsCollision(enemySphere, tri, pushVector)) {
                    if (std::abs(tri.normal.y) > 0.3f) {
                        float pushLen = MyMath::Length(pushVector);
                        if (pushLen < 0.05f) pushLen = 0.05f;
                        Vector3 groundNormal = tri.normal;
                        if (groundNormal.y < 0.0f) {
                            groundNormal = MyMath::Multiply(-1.0f, groundNormal);
                        }
                        pushVector = MyMath::Multiply(pushLen, groundNormal);
                    }
                    position_.x += pushVector.x;
                    position_.y += pushVector.y;
                    position_.z += pushVector.z;
                    enemySphere.center = position_;

                    if (std::abs(tri.normal.y) > 0.3f) {
                        hitGroundThisFrame = true;
                    }
                }
            }
        } else {
            OBB obsOBB = obs->GetOBB();
            if (std::abs(position_.x - obsOBB.center.x) <= obsOBB.size.x + scale_.x * 0.5f &&
                std::abs(position_.z - obsOBB.center.z) <= obsOBB.size.z + scale_.z * 0.5f) {
                float topY = obsOBB.center.y + obsOBB.size.y;
                if (position_.y <= topY + scale_.y) {
                    position_.y = topY + scale_.y;
                    hitGroundThisFrame = true;
                }
            }
        }
    }

    if (hitGroundThisFrame) {
        if (velocityY_ < 0.0f) {
            velocityY_ = 0.0f;
        }
        isGrounded_ = true;
    } else {
        isGrounded_ = false;
    }

    // スクリプト等で追跡を停止された場合だけ、Blenderで配置した位置を保つ。
    if (!isChasingPlayer_) {
        velocity_.x = 0.0f;
        velocity_.z = 0.0f;
        return;
    }

    // プレイヤーへの水平方向の旋回・進行

    Vector3 toPlayerHorizontal = { playerPos.x - position_.x, 0.0f, playerPos.z - position_.z };
    float distHorizontalSq = toPlayerHorizontal.x * toPlayerHorizontal.x + toPlayerHorizontal.z * toPlayerHorizontal.z;

    if (distHorizontalSq > 0.001f) {
        float distH = std::sqrt(distHorizontalSq);
        Vector3 targetDir = { toPlayerHorizontal.x / distH, 0.0f, toPlayerHorizontal.z / distH };
        
        // 進行方向の更新
        constexpr float turnRate = 0.08f;
        forward_.x += (targetDir.x - forward_.x) * turnRate;
        forward_.z += (targetDir.z - forward_.z) * turnRate;
        forward_ = NormalizeOrFallback(forward_, { 0.0f, 0.0f, 1.0f });

        // Y軸回転角の算出
        rotation_.y = std::atan2(forward_.x, forward_.z);
        rotation_.x = 0.0f; // 飛べないためピッチは水平固定
        rotation_.z = 0.0f;

        // 基本移動速度の設定（空中であってもプレイヤーへ接近・着地移動を行う）
        if (attackSubState_ == GroundAttackState::Melee || attackSubState_ == GroundAttackState::Gatling) {
            // 近接・ガトリング中は水平移動を行わない（MeleeはUpdateAttackAIで直接制御される）
            velocity_.x = 0.0f;
            velocity_.z = 0.0f;
        } else {
            // プレイヤーと一定距離（15.0f）を保つ
            constexpr float kMinKeepDistance = 15.0f;
            if (distHorizontalSq < kMinKeepDistance * kMinKeepDistance) {
                // 近すぎるので少し後退する
                velocity_.x = -forward_.x * kGroundSpeed * 0.5f;
                velocity_.z = -forward_.z * kGroundSpeed * 0.5f;
            } else {
                velocity_.x = forward_.x * kGroundSpeed;
                velocity_.z = forward_.z * kGroundSpeed;
            }
        }
    }

    // 空中での減衰（慣性の減衰）
    if (!isGrounded_) {
        velocity_.x *= 0.98f;
        velocity_.z *= 0.98f;
    }

    // 水平位置の更新（地上・空中問わず velocity_ を適用）
    position_.x += velocity_.x;
    position_.z += velocity_.z;

    // 上の接地判定は横移動「前」の座標を使っている。ここで移動先の地表を
    // 改めてサンプリングして足元を合わせることで、起伏のある地形でも
    // 1フレーム遅れで地面を抜けたり、斜面の下へ潜ったりしないようにする。
    // 飛び蹴りの跳躍中だけは着地させず、空中軌道を維持する。
    const bool isAirborneAttack = attackSubState_ == GroundAttackState::Melee &&
        (meleePhase_ == MeleePhase::Rising || meleePhase_ == MeleePhase::Falling);
    if (!isAirborneAttack) {
        Sphere terrainProbe{};
        terrainProbe.center = position_;
        terrainProbe.radius = (std::max)(0.5f, (std::max)(scale_.x, scale_.z));

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
                if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], position_.x, position_.z, terrainY) && terrainY > groundHeight) {
                    groundHeight = terrainY;
                    foundGroundAtDestination = true;
                }
            }
        }

        if (foundGroundAtDestination) {
            groundY_ = groundHeight;
            position_.y = groundY_ + GetFootOffset();
            velocityY_ = 0.0f;
            isGrounded_ = true;
        }
    }

    // 通常移動時はジャンプせず、地上での走行・旋回のみ行う（近接攻撃時にのみジャンプ飛び蹴りを実行する）
}

void GroundEnemy::UpdateAttackAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    Vector3 diff = { playerPos.x - position_.x, playerPos.y - position_.y, playerPos.z - position_.z };
    float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    float dist = std::sqrt(distSq);

    // 近接攻撃（飛び蹴りジャンプ降下攻撃）中の処理
    if (attackSubState_ == GroundAttackState::Melee) {
        if (meleePhase_ == MeleePhase::Rising) {
            // 跳躍上昇フェーズ（前方に跳び上がりつつ上昇。HIT判定はまだ無し）
            isMeleeActive_ = false;
            position_.x += forward_.x * (kGroundSpeed * 1.5f);
            position_.z += forward_.z * (kGroundSpeed * 1.5f);

            // 頂点付近（下降開始）に達したら急降下飛び蹴りフェーズへ
            if (velocityY_ <= 0.05f || !isGrounded_) {
                meleePhase_ = MeleePhase::Falling;
            }
        }
        else if (meleePhase_ == MeleePhase::Falling) {
            // 急降下・飛び蹴り中（降りてきたとき〜着地前までHIT判定ON！）
            isMeleeActive_ = true;
            position_.x += forward_.x * (kGroundSpeed * 2.0f);
            position_.z += forward_.z * (kGroundSpeed * 2.0f);
            velocityY_ -= gravity_ * 0.6f;

            // 地面に足が届いて着地したら攻撃判定OFF＆着地硬直フェーズへ
            if (isGrounded_) {
                isMeleeActive_ = false;
                meleePhase_ = MeleePhase::Landing;
                meleeLandingTimer_ = 18; // 18フレームの着地隙
            }
        }
        else if (meleePhase_ == MeleePhase::Landing) {
            // 着地硬直
            isMeleeActive_ = false;
            meleeLandingTimer_--;
            if (meleeLandingTimer_ <= 0) {
                meleeBoxPos_ = { -9999.0f, -9999.0f, -9999.0f };
                if (meleeBox_) {
                    meleeBox_->SetTranslate(meleeBoxPos_);
                    meleeBox_->Update();
                }
                attackSubState_ = GroundAttackState::Idle;
                attackTimer_ = 75; // 攻撃終了後のクールタイム
            }
        }
        return;
    }

    // ガトリング連射中の処理
    if (attackSubState_ == GroundAttackState::Gatling) {
        gatlingIntervalTimer_--;
        if (gatlingIntervalTimer_ <= 0 && bulletManager) {
            Vector3 muzzlePos = { position_.x + forward_.x * 0.45f, position_.y + 0.15f, position_.z + forward_.z * 0.45f };
            Vector3 shootDir = NormalizeOrFallback(diff, forward_);
            Vector3 bulletVelocity = { shootDir.x * 0.4f, shootDir.y * 0.4f, shootDir.z * 0.4f };
            
            bulletManager->Shoot(muzzlePos, bulletVelocity);
            gatlingShotCount_++;
            gatlingIntervalTimer_ = 10; // 弾幕が過密にならないよう、10フレーム毎に発射

            if (gatlingShotCount_ >= 4) { // 4連射で終了
                attackSubState_ = GroundAttackState::Idle;
                attackTimer_ = 120; // 次の射撃まで少し間を空ける
            }
        }
        return;
    }

    // 攻撃開始判断（クールタイム待ち）
    if (attackTimer_ > 0) {
        attackTimer_--;
        return;
    }

    // 距離に応じた攻撃の選択
    if (dist <= 10.0f) {
        // 1. 近距離: 近接跳び蹴り攻撃を発動（跳び上がって急降下蹴り）
        attackSubState_ = GroundAttackState::Melee;
        meleePhase_ = MeleePhase::Rising;
        isMeleeActive_ = false;
        velocityY_ = 0.38f; // 高く前方上にジャンピング
        isGrounded_ = false;
    } else if (dist <= 25.0f) {
        // 2. 中距離: ガトリング連射攻撃
        attackSubState_ = GroundAttackState::Gatling;
        gatlingShotCount_ = 0;
        gatlingIntervalTimer_ = 0;
    } else {
        // 3. 遠距離: VF3は誘導弾を使わない。距離を詰めるまで待機する。
        attackSubState_ = GroundAttackState::Idle;
        attackTimer_ = 90;
    }
}

void GroundEnemy::UpdateModel() {
    if (object_) {
        object_->SetTranslate(position_);
        object_->SetRotate(rotation_);
        object_->SetScale(scale_);

        // プレイヤーの変形キー(1, 2, 3)の影響を100%排除し、写真の人型バトロイド形態 (time = 5.0f) の姿を永久に固定
        if (enableSkinning_ && object_->GetModel()) {
            ApplyAnimation(skeleton_, animationData_, 5.0f);
            ::Update(skeleton_);
            object_->GetModel()->UpdateSkinCluster(skinCluster_, skeleton_);
            object_->skinCluster = skinCluster_;
        }

        object_->Update();
    }

    // 近接飛び蹴り攻撃用判定ブロックのトランスフォーム更新（降りてきた急降下中〜着地前）
    if (isMeleeActive_ && meleeBox_) {
        // 前方斜め下（飛び蹴り脚の先）に突き出す
        meleeBoxPos_ = {
            position_.x + forward_.x * 0.55f,
            position_.y - 0.08f,
            position_.z + forward_.z * 0.55f
        };
        meleeBox_->SetTranslate(meleeBoxPos_);
        meleeBox_->SetRotate(rotation_);
        meleeBox_->SetScale(meleeBoxScale_);
        meleeBox_->Update();
    }
}

void GroundEnemy::Draw() {
    if (object_) {
        object_->Draw();
    }

    // 近接攻撃中のみ攻撃判定ブロックを描画
    if (isMeleeActive_ && meleeBox_) {
        meleeBox_->Draw();
    }
}

OBB GroundEnemy::GetMeleeBoxOBB() const {
    OBB obb{};
    obb.center = meleeBoxPos_;
    obb.size = { meleeBoxScale_.x * 0.5f, meleeBoxScale_.y * 0.5f, meleeBoxScale_.z * 0.5f };

    Matrix4x4 rotationMatrix = MyMath::Multiply(
        MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::MakeRotateYMatrix(rotation_.y)),
        MyMath::MakeRotateZMatrix(rotation_.z));

    obb.orientations[0] = MyMath::Normalize(Vector3{ rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2] });
    obb.orientations[1] = MyMath::Normalize(Vector3{ rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2] });
    obb.orientations[2] = MyMath::Normalize(Vector3{ rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2] });

    return obb;
}

void GroundEnemy::SnapToGround(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    float maxGroundY = -99999.0f;
    bool foundGround = false;

    // 1. 直下の三角形の標高を探す
    for (const auto &obs : obstacles) {
        if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled()) continue;

        if (obs->IsUseMeshCollider()) {
            const auto& triangles = obs->GetWorldTriangles();
            Sphere terrainSample{};
            terrainSample.center = position_;
            terrainSample.radius = 0.25f;
            for (const size_t triangleIndex : obs->GetNearbyWorldTriangleIndices(terrainSample)) {
                const Triangle& tri = triangles[triangleIndex];
                if (std::abs(tri.normal.y) > 0.1f) {
                    float triY = 0.0f;
                    if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], position_.x, position_.z, triY)) {
                        if (triY > maxGroundY) {
                            maxGroundY = triY;
                            foundGround = true;
                        }
                    }
                }
            }
        }
    }

    // 2. 直下でヒットしなかった場合、周辺サンプリングで山肌の標高を判定
    if (!foundGround) {
        float offsets[8][2] = {
            {-3.0f, 0.0f}, {3.0f, 0.0f}, {0.0f, -3.0f}, {0.0f, 3.0f},
            {-6.0f, -6.0f}, {6.0f, -6.0f}, {-6.0f, 6.0f}, {6.0f, 6.0f}
        };
        for (int i = 0; i < 8; ++i) {
            float sampleX = position_.x + offsets[i][0];
            float sampleZ = position_.z + offsets[i][1];
            for (const auto &obs : obstacles) {
                if (!obs || obs->IsStageBounds() || !obs->IsCollisionEnabled()) continue;
                if (obs->IsUseMeshCollider()) {
                    const auto& triangles = obs->GetWorldTriangles();
                    Sphere terrainSample{};
                    terrainSample.center = { sampleX, position_.y, sampleZ };
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

    // 近傍グリッドは通常時の高速な検索用。スポーン地点が地形の内部に
    // 入っている場合でも、ここで地表を見失って Y=0 に落とさないよう、
    // 生成時だけ全三角形を使う最終フォールバックを行う。
    // SnapToGround() はスポーン時／リスポーン時にしか呼ばれないため、
    // この低頻度の検索はフレーム負荷には影響しない。
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
                if (GetTriangleY(tri.p[0], tri.p[1], tri.p[2], position_.x, position_.z, triY) && triY > maxGroundY) {
                    maxGroundY = triY;
                    foundGround = true;
                }
            }
        }
    }

    const float footOffset = GetFootOffset();
    if (foundGround) {
        position_.y = maxGroundY + footOffset;
    } else {
        // 地形の外などで地表が存在しない場合も、Y=0へ強制移動させず、
        // 配置データの高さを維持する。これにより地形の下へ落下しない。
        position_.y = (std::max)(position_.y, footOffset);
    }

    velocityY_ = 0.0f;
    isGrounded_ = true;
    UpdateModel();
}

float GroundEnemy::GetFootOffset() const {
    if (!object_ || !object_->GetModel()) {
        return scale_.y;
    }

    // Object3d の原点は足元とは限らない。モデルの最下端が地面の高さになる分だけ
    // 原点を持ち上げることで、地形の傾斜上にも確実に立たせる。
    const Vector3 center = object_->GetModel()->GetBoundsCenter();
    const Vector3 halfExtents = object_->GetModel()->GetHalfExtents();
    const float localBottomY = center.y - halfExtents.y;
    return (std::max)(0.0f, -localBottomY * std::abs(scale_.y));
}

void GroundEnemy::UpdateFlightPathMovement() {
    if (flightPathPoints_.size() < 2) return;

    const size_t segmentCount = flightPathLoop_ ? flightPathPoints_.size() : flightPathPoints_.size() - 1;
    if (segmentCount == 0) return;

    Vector3 oldPosition = position_;
    float remainingDistance = flightPathSpeed_;

    while (remainingDistance > 0.0f) {
        size_t nextIndex = ResolvePathIndex(static_cast<int>(flightPathSegmentIndex_ + 1), flightPathPoints_.size(), flightPathLoop_);
        Vector3 segmentVector = {
            flightPathPoints_[nextIndex].x - flightPathPoints_[flightPathSegmentIndex_].x,
            0.0f,
            flightPathPoints_[nextIndex].z - flightPathPoints_[flightPathSegmentIndex_].z
        };
        float segmentLength = std::sqrt(segmentVector.x * segmentVector.x + segmentVector.z * segmentVector.z);

        if (segmentLength < 0.0001f) {
            ++flightPathSegmentIndex_;
            flightPathSegmentT_ = 0.0f;
            if (flightPathSegmentIndex_ >= segmentCount) {
                if (flightPathLoop_) {
                    flightPathSegmentIndex_ = 0;
                } else {
                    flightPathSegmentIndex_ = segmentCount - 1;
                    flightPathSegmentT_ = 1.0f;
                    break;
                }
            }
            continue;
        }

        float distanceToSegmentEnd = segmentLength * (1.0f - flightPathSegmentT_);
        if (remainingDistance >= distanceToSegmentEnd) {
            remainingDistance -= distanceToSegmentEnd;
            flightPathSegmentT_ = 0.0f;
            ++flightPathSegmentIndex_;
            if (flightPathSegmentIndex_ >= segmentCount) {
                if (flightPathLoop_) {
                    flightPathSegmentIndex_ = 0;
                } else {
                    flightPathSegmentIndex_ = segmentCount - 1;
                    flightPathSegmentT_ = 1.0f;
                    break;
                }
            }
        } else {
            flightPathSegmentT_ += remainingDistance / segmentLength;
            remainingDistance = 0.0f;
        }
    }

    Vector3 targetPos = EvaluateFlightPath(flightPathPoints_, flightPathSegmentIndex_, flightPathSegmentT_, flightPathLoop_);
    position_.x = targetPos.x;
    position_.z = targetPos.z;

    Vector3 diff = { position_.x - oldPosition.x, 0.0f, position_.z - oldPosition.z };
    float diffLen = std::sqrt(diff.x * diff.x + diff.z * diff.z);
    if (diffLen > 0.001f) {
        forward_ = { diff.x / diffLen, 0.0f, diff.z / diffLen };
        rotation_.y = std::atan2(forward_.x, forward_.z);
    }

    velocity_.x = 0.0f;
    velocity_.z = 0.0f;
}
