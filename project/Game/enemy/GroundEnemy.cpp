#include "GroundEnemy.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"
#include "engine/Math/MyMath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

namespace {
    constexpr float kGroundSpeed = 0.08f;
    constexpr float kTrackSpeed = 0.16f;
    constexpr float kHighJumpPower = 0.38f;
    constexpr int kGroundEnemyMaxHp = 3;
    constexpr char kGroundEnemyRiggedModel[] = "EnemyR_Rig/EnemyR_Rig.gltf";
    // EnemyR は部位が別オブジェクトで出力されている。砲塔(38-48)だけを
    // 分離して独立照準させ、その他は元の形状のままメインボディとして描画する。
    constexpr char kGroundEnemyBodyModel[] = "EnemyR.obj#exclude=1-32,38-48";
    constexpr char kGroundEnemyTurretModel[] = "EnemyR.obj#groups=38-48;pivot=0.45,2.18,0.0";
    constexpr Vector3 kTurretPivot = { 0.45f, 2.18f, 0.0f };
    constexpr float kModelForwardYaw = 1.57079632679f;
    constexpr std::array<const char*, 4> kGroundEnemyLegModels = {
        "EnemyR.obj#groups=1-8;pivot=-0.62,1.35,0.76",
        "EnemyR.obj#groups=9-16;pivot=-0.62,1.35,-0.76",
        "EnemyR.obj#groups=17-24;pivot=0.80,1.35,-0.76",
        "EnemyR.obj#groups=25-32;pivot=0.80,1.35,0.76"
    };
    constexpr std::array<Vector3, 4> kLegPivots = {
        Vector3{ -0.62f, 1.35f, 0.76f }, Vector3{ -0.62f, 1.35f, -0.76f },
        Vector3{ 0.80f, 1.35f, -0.76f }, Vector3{ 0.80f, 1.35f, 0.76f }
    };
    constexpr std::array<Vector3, 4> kFootPositions = {
        Vector3{ -1.48f, 0.0f, 0.76f }, Vector3{ -1.48f, 0.0f, -0.76f },
        Vector3{ 0.98f, 0.0f, -0.76f }, Vector3{ 0.98f, 0.0f, 0.76f }
    };
    // ラゴゥ系の低い4拍ギャロップ。後脚が二本続けて胴体を押し、続いて前脚が
    // 着地するため、対角脚が同時に動く機械的なトロットにならない。
    constexpr std::array<float, 4> kGallopPhaseOffsets = {
        3.14159265359f, 4.20973415581f, 0.0f, 1.06814150222f
    };

    Vector3 NormalizeOrFallback(const Vector3 &v, const Vector3 &fallback) {
        float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq <= 0.0001f) {
            return fallback;
        }
        float len = std::sqrt(lenSq);
        return { v.x / len, v.y / len, v.z / len };
    }

    void AddRigJointRotation(Skeleton& skeleton, const char* jointName, const Vector3& axis, float angle) {
        const auto jointIt = skeleton.jointMap.find(jointName);
        if (jointIt == skeleton.jointMap.end()) {
            return;
        }
        const int32_t jointIndex = jointIt->second;
        if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton.joints.size())) {
            return;
        }
        Joint& joint = skeleton.joints[jointIndex];
        const Quaternion rotation = MyMath::MakeAxisAngle(axis, angle);
        joint.transform.rotate = MyMath::Normalize(MyMath::Multiply(joint.transform.rotate, rotation));
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
    if (modelMgr) {
        // PDFの手順で作成したアーマチュア付きglTFを優先する。読み込みに失敗した
        // 環境だけは従来OBJへ戻せるよう、既存の部位分割モデルをフォールバックに残す。
        if (!modelMgr->FindModel(kGroundEnemyRiggedModel)) {
            modelMgr->LoadModel(kGroundEnemyRiggedModel);
        }
        if (modelMgr->FindModel(kGroundEnemyRiggedModel)) {
            object_->SetModel(kGroundEnemyRiggedModel);
            usesRiggedModel_ = true;
        } else {
            if (!modelMgr->FindModel(kGroundEnemyBodyModel)) {
                modelMgr->LoadModel(kGroundEnemyBodyModel);
            }
            if (!modelMgr->FindModel(kGroundEnemyTurretModel)) {
                modelMgr->LoadModel(kGroundEnemyTurretModel);
            }
            for (const char* legModel : kGroundEnemyLegModels) {
                if (!modelMgr->FindModel(legModel)) {
                    modelMgr->LoadModel(legModel);
                }
            }
            object_->SetModel(kGroundEnemyBodyModel);
        }
    } else {
        object_->SetModel("EnemyBox"); // 初期化順異常時だけの安全なフォールバック
    }

    if (object_->GetModel()) {
        // 砂漠を疾走する獣型メカらしいオレンジ装甲と金属ハイライト。
        object_->GetModel()->SetColor({ 0.92f, 0.38f, 0.08f, 1.0f });
        object_->GetModel()->SetShininess(96.0f);
    }

    // EnemyR の実寸（約 6 x 2.6 x 6.3）に合わせる。
    scale_ = { 0.72f, 0.72f, 0.72f };
    visualScale_ = scale_;
    object_->SetScale(scale_);

    if (!usesRiggedModel_ && modelMgr && modelMgr->FindModel(kGroundEnemyTurretModel)) {
        turret_ = std::make_unique<Object3d>();
        turret_->Initialize(Object3dCommon::GetInstance());
        turret_->SetModel(kGroundEnemyTurretModel);
        if (turret_->GetModel()) {
            // 砲塔は装甲と区別できる青灰色にする。
            turret_->GetModel()->SetColor({ 0.16f, 0.32f, 0.42f, 1.0f });
            turret_->GetModel()->SetShininess(120.0f);
        }
    }
    if (!usesRiggedModel_ && modelMgr) {
        for (size_t i = 0; i < legs_.size(); ++i) {
            legs_[i] = std::make_unique<Object3d>();
            legs_[i]->Initialize(Object3dCommon::GetInstance());
            legs_[i]->SetModel(kGroundEnemyLegModels[i]);
            if (legs_[i]->GetModel()) {
                legs_[i]->GetModel()->SetColor({ 0.92f, 0.38f, 0.08f, 1.0f });
                legs_[i]->GetModel()->SetShininess(96.0f);
            }
        }
    }

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
    terrainClearance_ = 0.0f;

    isDead_ = false;
    hp_ = kGroundEnemyMaxHp;
    destructionState_ = DestructionState::Alive;
    hitStopFrames_ = 0;
    hitReactionFrames_ = 0;
    hitLean_ = 0.0f;
    skidYawVelocity_ = 0.0f;
    skidVelocity_ = { 0.0f, 0.0f, 0.0f };
    legMotionBlend_ = 0.0f;
    destructionFrames_ = 0;
    finalExplosionQueued_ = false;
    turretDetached_ = false;
    legDetached_.fill(false);
    pendingEffectEvents_.clear();
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

    rigBindPose_.clear();
    if (usesRiggedModel_ && object_->GetModel()) {
        rigSkeleton_ = CreateSkeleton(object_->GetModel()->GetModelData().rootNode);
        rigBindPose_.reserve(rigSkeleton_.joints.size());
        for (const Joint& joint : rigSkeleton_.joints) {
            rigBindPose_.push_back(joint.transform);
        }
        object_->skinCluster = object_->GetModel()->CreateSkinCluster(rigSkeleton_);
        if (!object_->skinCluster.isValid) {
            // ここで OBJ フォールバックへ切り替えると、初期化済みの脚オブジェクトが
            // 無い状態になってしまう。スキン更新だけを止め、モデルはバインド姿勢で
            // 描画を継続する。
            rigBindPose_.clear();
        }
    }

    object_->SetTranslate(position_);
    object_->SetRotate(rotation_);
    object_->Update();
}

void GroundEnemy::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    if (isDead_) return;

    // 撃破後も即座に削除せず、慣性滑走と誘爆が完了するまで表示し続ける。
    if (destructionState_ != DestructionState::Alive) {
        UpdateDestruction();
        UpdateDamageGlow();
        UpdateModel();
        return;
    }

    // 数フレームだけ入力・AIを止めるヒットストップ。見た目の沈み込みだけを残す。
    if (hitStopFrames_ > 0) {
        --hitStopFrames_;
        UpdateDamageGlow();
        UpdateModel();
        return;
    }

    if (hitReactionFrames_ > 0) {
        UpdateHitReaction(obstacles);
        UpdateDamageGlow();
        UpdateModel();
        return;
    }

    // 1. 地上移動とハイジャンプ・重力物理
    UpdateGroundMovement(playerPos, obstacles);

    // 2. 攻撃AI（ミサイル、ガトリング、近接攻撃）
    if (isChasingPlayer_) {
        UpdateAttackAI(playerPos, bulletManager);
    }
    turretTarget_ = playerPos;

    // 3. 当たり判定（障害物押し出し）
    // UpdateGroundMovement() 内で地形への接地まで済ませているため、ここで
    // 全く同じ地形検索をもう一度行わない。
    CheckCollision(obstacles);

    // 4. モデルの更新
    UpdateDamageGlow();
    UpdateModel();
}

bool GroundEnemy::CanReceiveDamage() const {
    return !isDead_ && destructionState_ == DestructionState::Alive;
}

std::vector<GroundEnemy::EffectEvent> GroundEnemy::ConsumeEffectEvents() {
    std::vector<EffectEvent> events = std::move(pendingEffectEvents_);
    pendingEffectEvents_.clear();
    return events;
}

void GroundEnemy::QueueEffect(EffectEvent::Type type, const Vector3 &position, const Vector3 &direction) {
    pendingEffectEvents_.push_back({ type, position, direction });
}

void GroundEnemy::TakeDamage(int damage, const Vector3 &hitDirection) {
    if (!CanReceiveDamage() || damage <= 0) {
        return;
    }

    // 弾丸の進行方向が取れない攻撃は、現在の進行方向を安全な代替に使う。
    hitDirection_ = NormalizeOrFallback(hitDirection, forward_);
    // 中心座標ではなく弾が入射した側の装甲表面に置く。
    // これで胴体に隠れず、どの方向から撃っても火花が見える。
    const float armorRadius = std::clamp(GetCollisionRadius() * 0.62f, 0.80f, 1.45f);
    const Vector3 impactPosition = {
        position_.x - hitDirection_.x * armorRadius,
        position_.y - hitDirection_.y * armorRadius + 0.18f,
        position_.z - hitDirection_.z * armorRadius,
    };
    QueueEffect(EffectEvent::Type::ArmorSpark, impactPosition, hitDirection_);

    // 装甲に弾かれた反動。履帯時だけ横滑りの慣性を強める。
    hitStopFrames_ = 4;
    hitReactionFrames_ = 20;
    hitLean_ = 1.0f;
    const float side = forward_.x * hitDirection_.z - forward_.z * hitDirection_.x;
    skidYawVelocity_ = std::clamp(side * 0.055f, -0.055f, 0.055f);
    skidVelocity_ = {
        velocity_.x * 0.82f + hitDirection_.x * 0.045f,
        0.0f,
        velocity_.z * 0.82f + hitDirection_.z * 0.045f,
    };
    if (trackBlend_ > 0.15f) {
        QueueEffect(EffectEvent::Type::TrackSkid, position_, skidVelocity_);
    }

    hp_ -= damage;
    if (hp_ <= 0) {
        hp_ = 0;
        BeginDestruction(hitDirection_);
    }
}

void GroundEnemy::UpdateHitReaction(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    position_.x += skidVelocity_.x;
    position_.z += skidVelocity_.z;
    skidVelocity_.x *= trackBlend_ > 0.1f ? 0.91f : 0.78f;
    skidVelocity_.z *= trackBlend_ > 0.1f ? 0.91f : 0.78f;
    rotation_.y += skidYawVelocity_;
    skidYawVelocity_ *= 0.80f;
    forward_ = { std::sin(rotation_.y), 0.0f, std::cos(rotation_.y) };
    hitLean_ *= 0.87f;
    --hitReactionFrames_;
    UpdateTerrainPose(obstacles);
}

Vector3 GroundEnemy::GetLocalPartPosition(const Vector3 &localPosition) const {
    const float yaw = visualRotation_.y;
    const float c = std::cos(yaw);
    const float s = std::sin(yaw);
    const float localX = localPosition.x * visualScale_.x;
    const float localZ = localPosition.z * visualScale_.z;
    return {
        position_.x + localX * c + localZ * s,
        position_.y + terrainClearance_ + visualHeightOffset_ + localPosition.y * visualScale_.y,
        position_.z - localX * s + localZ * c,
    };
}

void GroundEnemy::BeginDestruction(const Vector3 &hitDirection) {
    destructionState_ = DestructionState::Sliding;
    destructionFrames_ = 0;
    isChasingPlayer_ = false;
    isMeleeActive_ = false;
    attackSubState_ = GroundAttackState::Idle;
    hitStopFrames_ = 0;
    hitReactionFrames_ = 0;
    hitLean_ = 1.0f;
    velocity_ = {
        velocity_.x + hitDirection.x * 0.055f,
        0.0f,
        velocity_.z + hitDirection.z * 0.055f,
    };
    if (std::abs(velocity_.x) + std::abs(velocity_.z) < 0.03f) {
        velocity_.x = forward_.x * 0.10f;
        velocity_.z = forward_.z * 0.10f;
    }
    QueueEffect(EffectEvent::Type::TrackSkid, position_, velocity_);
}

void GroundEnemy::UpdateDetachedParts() {
    const float floorY = groundY_ + 0.08f;
    auto updatePart = [floorY](Vector3 &partPosition, Vector3 &partVelocity, Vector3 &partRotation) {
        partVelocity.y -= 0.018f;
        partPosition.x += partVelocity.x;
        partPosition.y += partVelocity.y;
        partPosition.z += partVelocity.z;
        partRotation.x += 0.10f;
        partRotation.y += 0.15f;
        partRotation.z += 0.07f;
        if (partPosition.y < floorY) {
            partPosition.y = floorY;
            partVelocity.y = -partVelocity.y * 0.24f;
            partVelocity.x *= 0.79f;
            partVelocity.z *= 0.79f;
        }
    };

    if (turretDetached_) {
        updatePart(turretDebrisPosition_, turretDebrisVelocity_, turretDebrisRotation_);
        if (turret_) {
            turret_->SetTranslate(turretDebrisPosition_);
            turret_->SetRotate(turretDebrisRotation_);
            turret_->SetScale(visualScale_);
            turret_->Update();
        }
    }
    for (size_t i = 0; i < legs_.size(); ++i) {
        if (!legDetached_[i]) continue;
        updatePart(legDebrisPositions_[i], legDebrisVelocities_[i], legDebrisRotations_[i]);
        if (legs_[i]) {
            legs_[i]->SetTranslate(legDebrisPositions_[i]);
            legs_[i]->SetRotate(legDebrisRotations_[i]);
            legs_[i]->SetScale(visualScale_);
            legs_[i]->Update();
        }
    }
}

void GroundEnemy::UpdateDestruction() {
    ++destructionFrames_;
    position_.x += velocity_.x;
    position_.z += velocity_.z;
    velocity_.x *= 0.925f;
    velocity_.z *= 0.925f;
    rotation_.y += skidYawVelocity_;
    skidYawVelocity_ *= 0.86f;
    forward_ = { std::sin(rotation_.y), 0.0f, std::cos(rotation_.y) };

    if (destructionFrames_ == 8 && !turretDetached_) {
        turretDetached_ = true;
        turretDebrisPosition_ = GetLocalPartPosition(kTurretPivot);
        turretDebrisVelocity_ = { velocity_.x + hitDirection_.x * 0.13f, 0.20f, velocity_.z + hitDirection_.z * 0.13f };
        turretDebrisRotation_ = visualRotation_;
    }
    if (turretDetached_ && (destructionFrames_ == 13 || destructionFrames_ == 25 || destructionFrames_ == 37)) {
        const Vector3 misfireDirection = NormalizeOrFallback({ hitDirection_.z, 0.18f, -hitDirection_.x }, forward_);
        QueueEffect(EffectEvent::Type::TurretMisfire, turretDebrisPosition_, misfireDirection);
    }
    if (destructionFrames_ == 20 || destructionFrames_ == 32) {
        const size_t firstLeg = destructionFrames_ == 20 ? 0 : 2;
        for (size_t i = firstLeg; i < firstLeg + 2; ++i) {
            legDetached_[i] = true;
            legDebrisPositions_[i] = GetLocalPartPosition(kLegPivots[i]);
            const float side = (i % 2 == 0) ? 1.0f : -1.0f;
            legDebrisVelocities_[i] = { velocity_.x + side * 0.11f, 0.13f + static_cast<float>(i) * 0.015f, velocity_.z - hitDirection_.z * 0.07f };
            legDebrisRotations_[i] = visualRotation_;
        }
    }
    if (destructionFrames_ == 14 || destructionFrames_ == 27 || destructionFrames_ == 40 || destructionFrames_ == 53) {
        destructionState_ = DestructionState::ChainDetonating;
        const int index = destructionFrames_ / 13;
        const Vector3 local = {
            index % 2 == 0 ? -0.52f : 0.70f,
            1.15f + static_cast<float>(index % 2) * 0.42f,
            index % 3 == 0 ? -0.58f : 0.54f,
        };
        QueueEffect(EffectEvent::Type::ChainDetonation, GetLocalPartPosition(local), hitDirection_);
    }

    UpdateDetachedParts();

    if (destructionFrames_ >= 66 && !finalExplosionQueued_) {
        finalExplosionQueued_ = true;
        QueueEffect(EffectEvent::Type::FinalExplosion, position_, hitDirection_);
        isDead_ = true;
    }
}

void GroundEnemy::UpdateDamageGlow() {
    float glow = 0.0f;
    if (destructionState_ == DestructionState::ChainDetonating) {
        glow = 0.65f + 0.55f * std::abs(std::sin(static_cast<float>(destructionFrames_) * 1.9f));
    } else if (hitReactionFrames_ > 0 || hitStopFrames_ > 0) {
        glow = 0.28f + hitLean_ * 0.50f;
    }

    auto setGlow = [glow](Object3d *part) {
        if (!part) return;
        if (DirectionalLight *light = part->GetDirectionalLightData()) {
            light->color = glow > 0.0f ? Vector4{ 1.0f, 0.33f + glow * 0.22f, 0.06f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
            light->intensity = 1.0f + glow * 2.4f;
        }
    };
    setGlow(object_.get());
    setGlow(turret_.get());
    for (const auto &leg : legs_) setGlow(leg.get());
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
                // 歩行面への接地は上の高さサンプルだけで行う。ここで球を斜面法線に
                // 沿って押し戻すと、前進成分まで相殺されて地面に引っ掛かったように見える。
                if (std::abs(tri.normal.y) > 0.3f) {
                    continue;
                }
                Vector3 pushVector;
                if (MyMath::IsCollision(enemySphere, tri, pushVector)) {
                    position_.x += pushVector.x;
                    position_.y += pushVector.y;
                    position_.z += pushVector.z;
                    enemySphere.center = position_;
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
        UpdateTerrainPose(obstacles);
        return;
    }

    // プレイヤーへの水平方向の旋回・進行

    Vector3 toPlayerHorizontal = { playerPos.x - position_.x, 0.0f, playerPos.z - position_.z };
    float distHorizontalSq = toPlayerHorizontal.x * toPlayerHorizontal.x + toPlayerHorizontal.z * toPlayerHorizontal.z;

    if (distHorizontalSq > 0.001f) {
        float distH = std::sqrt(distHorizontalSq);
        Vector3 targetDir = { toPlayerHorizontal.x / distH, 0.0f, toPlayerHorizontal.z / distH };

        // 遠距離では脚を畳んだ低姿勢の履帯モードへ。近距離に入ると四足へ戻す。
        // 近接跳躍中は必ず四足モードを維持する。
        locomotionMode_ = (attackSubState_ != GroundAttackState::Melee && distH > 20.0f)
            ? LocomotionMode::Track
            : LocomotionMode::Quadruped;
        const float targetTrackBlend = locomotionMode_ == LocomotionMode::Track ? 1.0f : 0.0f;
        trackBlend_ += (targetTrackBlend - trackBlend_) * 0.08f;
        
        // 履帯走行は超信地旋回せず、前進しながら緩く曲がる。
        const float turnRate = 0.08f - trackBlend_ * 0.05f;
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
            const float moveSpeed = kGroundSpeed + (kTrackSpeed - kGroundSpeed) * trackBlend_;
            const float directionSign = distHorizontalSq < kMinKeepDistance * kMinKeepDistance ? -0.5f : 1.0f;
            if (trackBlend_ > 0.01f) {
                // 低摩擦の履帯モードでは横方向の慣性を残し、緩いドリフトを作る。
                const Vector3 targetVelocity = { forward_.x * moveSpeed * directionSign, 0.0f, forward_.z * moveSpeed * directionSign };
                const float traction = 0.08f + (1.0f - trackBlend_) * 0.20f;
                velocity_.x += (targetVelocity.x - velocity_.x) * traction;
                velocity_.z += (targetVelocity.z - velocity_.z) * traction;
            } else if (distHorizontalSq < kMinKeepDistance * kMinKeepDistance) {
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

    UpdateTerrainPose(obstacles);

    // 通常移動時はジャンプせず、地上での走行・旋回のみ行う（近接攻撃時にのみジャンプ飛び蹴りを実行する）
}

void GroundEnemy::UpdateTerrainPose(const std::list<std::unique_ptr<Obstacle>>& obstacles) {
    // 前後・左右の4点をサンプリングし、脚IKの代わりに機体全体を地形へ追従させる。
    // 静的OBJを変形させないので、急斜面でも各パーツの接続が崩れない。
    Vector3 flatForward = NormalizeOrFallback({ forward_.x, 0.0f, forward_.z }, { 0.0f, 0.0f, 1.0f });
    Vector3 right = { flatForward.z, 0.0f, -flatForward.x };
    constexpr float kProbeDistance = 1.0f;
    float front = 0.0f, back = 0.0f, left = 0.0f, rightHeight = 0.0f;
    const bool hasFront = SampleTerrainHeight(obstacles, { position_.x + flatForward.x * kProbeDistance, position_.y, position_.z + flatForward.z * kProbeDistance }, front);
    const bool hasBack = SampleTerrainHeight(obstacles, { position_.x - flatForward.x * kProbeDistance, position_.y, position_.z - flatForward.z * kProbeDistance }, back);
    const bool hasLeft = SampleTerrainHeight(obstacles, { position_.x - right.x * kProbeDistance, position_.y, position_.z - right.z * kProbeDistance }, left);
    const bool hasRight = SampleTerrainHeight(obstacles, { position_.x + right.x * kProbeDistance, position_.y, position_.z + right.z * kProbeDistance }, rightHeight);

    const float pitchTarget = hasFront && hasBack ? std::atan2(front - back, kProbeDistance * 2.0f) : 0.0f;
    const float rollTarget = hasLeft && hasRight ? std::atan2(rightHeight - left, kProbeDistance * 2.0f) : 0.0f;
    terrainPitch_ += (pitchTarget - terrainPitch_) * 0.16f;
    terrainRoll_ += (rollTarget - terrainRoll_) * 0.16f;

    // 脚をそれぞれ別の地面高さへ移動すると、静的メッシュの接続が離れて見える。
    // 全パーツを一つの剛体として同じ地形姿勢に乗せ、傾斜の低い側だけを
    // 共通のリフト量で逃がす。これならボーンのないモデルでも胴体と脚がばらけない。
    const bool hasTerrain = hasFront || hasBack || hasLeft || hasRight;
    if (hasTerrain) {
        const Vector3 terrainRotation = { terrainRoll_, rotation_.y + kModelForwardYaw, terrainPitch_ };
        const Matrix4x4 terrainMatrix = MyMath::MakeAffineMatrix(
            { 1.0f, 1.0f, 1.0f }, terrainRotation, { 0.0f, 0.0f, 0.0f });
        const float footOffset = GetFootOffset();
        float lowestSupportY = 0.0f;
        for (const Vector3& foot : kFootPositions) {
            const Vector3 rotatedFoot = MyMath::Transform({
                foot.x * scale_.x,
                -footOffset,
                foot.z * scale_.z,
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
            if (velocityY_ <= 0.05f) {
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

void GroundEnemy::UpdateLocomotionPose() {
    // 衝突形状は scale_ / rotation_ のまま固定し、描画だけに重心移動を加える。
    // そのため元OBJの各パーツを変形・分解せず、足元の接地判定も安定する。
    const float horizontalSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    const float targetLegMotion = horizontalSpeed > 0.025f && destructionState_ == DestructionState::Alive ? 1.0f : 0.0f;
    legMotionBlend_ += (targetLegMotion - legMotionBlend_) * 0.18f;
    // 完全停止時には位相も止める。振幅だけを先にゼロへ補間するため、足が急に折れない。
    gaitPhase_ += legMotionBlend_ * (0.024f + std::clamp(horizontalSpeed / kGroundSpeed, 0.0f, 1.0f) * 0.245f);
    const float runBlend = std::clamp(horizontalSpeed / kGroundSpeed, 0.0f, 1.0f) *
        (1.0f - trackBlend_) * legMotionBlend_;

    // EnemyR の長手方向(X)をゲーム内の前方(Z)へ揃える。
    // 地形の前後傾斜はモデルの長手方向まわりに、左右傾斜は横方向まわりに反映する。
    visualRotation_ = { terrainRoll_, rotation_.y + kModelForwardYaw, terrainPitch_ };
    visualScale_ = scale_;
    visualHeightOffset_ = std::sin(gaitPhase_ * 0.5f) * 0.018f * legMotionBlend_;

    if (hitStopFrames_ > 0 || hitReactionFrames_ > 0) {
        // 被弾側へ前脚を畳み、重心を落とす。静的メッシュでも一拍「ガクッ」とする。
        const float crouch = std::clamp(hitLean_, 0.0f, 1.0f);
        visualHeightOffset_ -= crouch * 0.14f;
        visualScale_.y *= 1.0f - crouch * 0.14f;
        visualRotation_.z += (hitDirection_.x * forward_.z - hitDirection_.z * forward_.x) * crouch * 0.18f;
        visualRotation_.x += crouch * 0.11f;
    }

    if (destructionState_ != DestructionState::Alive) {
        // 倒れ込みながら前へ滑らせる。完全停止までは胴体を地面へ近づける。
        const float collapse = std::clamp(static_cast<float>(destructionFrames_) / 42.0f, 0.0f, 1.0f);
        visualHeightOffset_ -= 0.12f + collapse * 0.42f;
        visualRotation_.z += 0.18f + collapse * 0.62f;
        visualRotation_.x += std::sin(static_cast<float>(destructionFrames_) * 0.35f) * (1.0f - collapse) * 0.07f;
        visualScale_.y *= 1.0f - collapse * 0.18f;
    }

    if (attackSubState_ == GroundAttackState::Melee) {
        if (meleePhase_ == MeleePhase::Rising) {
            // 跳び掛かる前傾姿勢。
            visualRotation_.z -= 0.28f;
            visualScale_.x *= 1.08f;
            visualScale_.y *= 0.88f;
            visualScale_.z *= 1.14f;
            visualHeightOffset_ -= 0.10f;
        } else if (meleePhase_ == MeleePhase::Falling) {
            // 急降下時は前脚を突き出すように機体を前へ倒す。
            visualRotation_.z += 0.38f;
            visualScale_.x *= 1.06f;
            visualScale_.y *= 0.92f;
            visualScale_.z *= 1.12f;
        } else {
            // 着地の脚サスペンションを縮める。
            visualScale_.x *= 1.10f;
            visualScale_.y *= 0.78f;
            visualScale_.z *= 1.10f;
            visualHeightOffset_ -= 0.09f;
        }
        return;
    }

    if (trackBlend_ > 0.01f) {
        // 履帯モード: 脚は頂点変形せず、低姿勢化とUVスクロールで高速滑走を表現する。
        visualScale_.x *= 1.0f + trackBlend_ * 0.10f;
        visualScale_.y *= 1.0f - trackBlend_ * 0.24f;
        visualScale_.z *= 1.0f + trackBlend_ * 0.05f;
        visualHeightOffset_ -= trackBlend_ * 0.11f;
        trackUvOffset_ += horizontalSpeed * (0.20f + trackBlend_ * 0.80f);
        if (object_ && object_->GetModel()) {
            object_->GetModel()->SetUvTransform(MyMath::MakeTranslateMatrix({ trackUvOffset_, 0.0f, 0.0f }));
        }
    }

    if (runBlend > 0.05f) {
        // 低いギャロップ: 足が集まる瞬間に胴体を沈め、蹴り出しで前へ伸ばす。
        // 参照動画のように接地感を残しつつ、走行速度を強く見せる。
        const float compression = (1.0f - std::cos(gaitPhase_ * 2.0f)) * 0.5f;
        visualHeightOffset_ -= compression * 0.052f * runBlend;
        visualHeightOffset_ += std::sin(gaitPhase_ - 0.45f) * 0.022f * runBlend;
        visualRotation_.z += std::sin(gaitPhase_ - 0.22f) * 0.145f * runBlend;
        visualRotation_.x += std::sin(gaitPhase_ * 2.0f) * 0.055f * runBlend;
        visualScale_.y *= 1.0f - compression * 0.075f * runBlend;
        visualScale_.z *= 1.0f + (1.0f - compression) * 0.055f * runBlend;
    }
}

void GroundEnemy::UpdateRiggedPose() {
    if (!usesRiggedModel_ || !object_ || !object_->GetModel() ||
        !object_->skinCluster.isValid || rigBindPose_.size() != rigSkeleton_.joints.size()) {
        return;
    }

    // 毎フレーム必ずバインド姿勢へ戻してから角度だけを加える。前フレームの回転を
    // 積み重ねないため、停止中に脚がバタついたり、関節がねじれ続けたりしない。
    for (size_t i = 0; i < rigSkeleton_.joints.size(); ++i) {
        rigSkeleton_.joints[i].transform = rigBindPose_[i];
    }

    const float horizontalSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    const float runBlend = std::clamp(horizontalSpeed / kGroundSpeed, 0.0f, 1.0f) *
        (1.0f - trackBlend_) * legMotionBlend_;
    constexpr std::array<const char*, 4> kRigLegNames = {
        "FrontRight", "FrontLeft", "RearLeft", "RearRight"
    };

    for (size_t i = 0; i < kRigLegNames.size(); ++i) {
        const float phase = gaitPhase_ + kGallopPhaseOffsets[i];
        const float stride = std::sin(phase) * runBlend;
        const float swing = (std::max)(0.0f, stride);
        const float drive = (std::max)(0.0f, -stride);
        // 後脚は地面を強く押し、前脚は着地時に大きく膝を畳む。各角度は親子ボーン
        // だけに適用されるので、装甲板と足先が胴体から独立して動くことはない。
        const float legPower = i < 2 ? 0.88f : 1.15f;
        float hipAngle = (swing * 0.43f - drive * 0.31f) * legPower;
        float kneeAngle = -swing * 0.72f + drive * 0.12f;
        float ankleAngle = swing * 0.24f - drive * 0.18f;
        float toeAngle = -swing * 0.10f + drive * 0.08f;

        if (trackBlend_ > 0.01f) {
            // 履帯モードでは全脚を内側へ畳み、歩行用の振りを止める。
            hipAngle += (i < 2 ? -0.60f : 0.42f) * trackBlend_;
            kneeAngle += (i < 2 ? 0.28f : -0.18f) * trackBlend_;
        }
        if (hitStopFrames_ > 0 || hitReactionFrames_ > 0) {
            // 被弾側の前脚から荷重が抜ける短い屈服。根元だけではなく膝・足首も
            // 同じチェーンで曲がるため、ピン脚のような不自然さを出さない。
            const float frontLegWeight = i < 2 ? 1.0f : 0.38f;
            const float side = i % 2 == 0 ? -1.0f : 1.0f;
            hipAngle += side * hitLean_ * frontLegWeight * 0.32f;
            kneeAngle -= hitLean_ * frontLegWeight * 0.34f;
            ankleAngle += hitLean_ * frontLegWeight * 0.14f;
        }

        const std::string prefix = kRigLegNames[i];
        AddRigJointRotation(rigSkeleton_, (prefix + "_Hip").c_str(), { 0.0f, 0.0f, 1.0f }, hipAngle);
        AddRigJointRotation(rigSkeleton_, (prefix + "_Knee").c_str(), { 0.0f, 0.0f, 1.0f }, kneeAngle);
        AddRigJointRotation(rigSkeleton_, (prefix + "_Ankle").c_str(), { 0.0f, 0.0f, 1.0f }, ankleAngle);
        AddRigJointRotation(rigSkeleton_, (prefix + "_Toe").c_str(), { 0.0f, 0.0f, 1.0f }, toeAngle);
    }

    // 独立 OBJ だった砲塔も同じスケルトン上で旋回させる。
    const Vector3 toTarget = { turretTarget_.x - position_.x, 0.0f, turretTarget_.z - position_.z };
    if (toTarget.x * toTarget.x + toTarget.z * toTarget.z > 0.001f) {
        const float desiredWorldYaw = std::atan2(toTarget.x, toTarget.z);
        auto wrapRadians = [](float angle) {
            constexpr float kPi = 3.14159265359f;
            while (angle > kPi) angle -= kPi * 2.0f;
            while (angle < -kPi) angle += kPi * 2.0f;
            return angle;
        };
        const float targetRelativeYaw = std::clamp(wrapRadians(desiredWorldYaw - rotation_.y), -1.25f, 1.25f);
        turretYaw_ += wrapRadians(targetRelativeYaw - turretYaw_) * 0.16f;
    }
    if (!turretDetached_) {
        AddRigJointRotation(rigSkeleton_, "Turret", { 0.0f, 1.0f, 0.0f }, turretYaw_);
    }

    ::Update(rigSkeleton_);
    object_->GetModel()->UpdateSkinCluster(object_->skinCluster, rigSkeleton_);
}

void GroundEnemy::UpdateTurretPose() {
    if (!turret_ || turretDetached_) return;

    const Vector3 toTarget = { turretTarget_.x - position_.x, 0.0f, turretTarget_.z - position_.z };
    if (toTarget.x * toTarget.x + toTarget.z * toTarget.z > 0.001f) {
        const float desiredWorldYaw = std::atan2(toTarget.x, toTarget.z);
        auto wrapRadians = [](float angle) {
            constexpr float kPi = 3.14159265359f;
            while (angle > kPi) angle -= kPi * 2.0f;
            while (angle < -kPi) angle += kPi * 2.0f;
            return angle;
        };
        const float targetRelativeYaw = std::clamp(wrapRadians(desiredWorldYaw - rotation_.y), -1.25f, 1.25f);
        turretYaw_ += wrapRadians(targetRelativeYaw - turretYaw_) * 0.16f;
    }

    // 分離OBJは砲塔の旋回支点を原点としてロード済み。メインボディ上の元位置へ戻してから回転する。
    const float c = std::cos(visualRotation_.y);
    const float s = std::sin(visualRotation_.y);
    const float pivotX = kTurretPivot.x * visualScale_.x;
    const float pivotZ = kTurretPivot.z * visualScale_.z;
    const Vector3 turretPosition = {
        position_.x + pivotX * c + pivotZ * s,
        position_.y + terrainClearance_ + visualHeightOffset_ + kTurretPivot.y * visualScale_.y,
        position_.z - pivotX * s + pivotZ * c
    };
    Vector3 turretRotation = visualRotation_;
    turretRotation.y += turretYaw_;
    turret_->SetTranslate(turretPosition);
    turret_->SetRotate(turretRotation);
    turret_->SetScale(visualScale_);
    turret_->Update();
}

void GroundEnemy::UpdateLegsPose() {
    // 脚フレーム・膝装甲・足先は同じ脚モデルに含まれる。別オブジェクトへ分けて
    // 回転すると接続が外れるので、脚一本を常に単一の剛体として扱う。
    const float c = std::cos(visualRotation_.y);
    const float s = std::sin(visualRotation_.y);
    for (size_t i = 0; i < legs_.size(); ++i) {
        if (legDetached_[i] || !legs_[i]) continue;

        const float phase = gaitPhase_ + kGallopPhaseOffsets[i];
        const float stride = std::sin(phase) * (1.0f - trackBlend_) * legMotionBlend_;
        const float swing = (std::max)(0.0f, stride);
        const float drive = (std::max)(0.0f, -stride);
        // 後脚を少し強く振り、胴体を押し出す重い低姿勢のギャロップにする。
        const float legPower = i < 2 ? 0.88f : 1.15f;
        float hipAngle = (swing * 0.43f - drive * 0.31f) * legPower;
        // 履帯形態では脚を同じ方向へ畳み、通常の歩行用屈伸は消す。
        hipAngle += (i < 2 ? -0.60f : 0.42f) * trackBlend_;

        Vector3 legPosition = {
            position_.x + kLegPivots[i].x * visualScale_.x * c + kLegPivots[i].z * visualScale_.z * s,
            position_.y + terrainClearance_ + visualHeightOffset_ + kLegPivots[i].y * visualScale_.y,
            position_.z - kLegPivots[i].x * visualScale_.x * s + kLegPivots[i].z * visualScale_.z * c,
        };
        if (hitStopFrames_ > 0 || hitReactionFrames_ > 0) {
            // 前脚ほど大きく屈服させ、被弾の荷重が脚へ落ちたように見せる。
            const float frontLegWeight = i < 2 ? 1.0f : 0.38f;
            hipAngle += (i % 2 == 0 ? -1.0f : 1.0f) * hitLean_ * frontLegWeight * 0.32f;
            legPosition.y -= hitLean_ * frontLegWeight * 0.08f;
        }

        Vector3 legRotation = visualRotation_;
        legRotation.z += hipAngle;
        Vector3 legScale = visualScale_;
        legScale.y *= 1.0f - trackBlend_ * 0.45f;
        legs_[i]->SetTranslate(legPosition);
        legs_[i]->SetRotate(legRotation);
        legs_[i]->SetScale(legScale);
        legs_[i]->Update();
    }
}

void GroundEnemy::UpdateModel() {
    UpdateLocomotionPose();
    UpdateRiggedPose();
    if (object_) {
        object_->SetTranslate({ position_.x, position_.y + terrainClearance_ + visualHeightOffset_, position_.z });
        object_->SetRotate(visualRotation_);
        object_->SetScale(visualScale_);
        object_->Update();
    }
    if (!usesRiggedModel_) {
        UpdateLegsPose();
        UpdateTurretPose();
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
    if (turret_) {
        turret_->Draw();
    }
    for (const auto& leg : legs_) {
        if (leg) {
            leg->Draw();
        }
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
    float localBottomY = center.y - halfExtents.y;

    // 胴体だけでなく分離描画している4脚の最下端まで含める。
    // 胴体のバウンド計算だけでは脚先が地面に埋まってしまうため、ここが接地基準になる。
    for (size_t i = 0; i < legs_.size(); ++i) {
        if (!legs_[i] || !legs_[i]->GetModel()) {
            continue;
        }
        const Vector3 legCenter = legs_[i]->GetModel()->GetBoundsCenter();
        const Vector3 legHalfExtents = legs_[i]->GetModel()->GetHalfExtents();
        localBottomY = (std::min)(localBottomY, kLegPivots[i].y + legCenter.y - legHalfExtents.y);
    }
    return (std::max)(0.0f, -localBottomY * std::abs(scale_.y)) + 0.025f;
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
