#include "GroundEnemy.h"
#include "GroundEnemyDestructionController.h"
#include "GroundEnemyMovementController.h"
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
    // ビーム射程（25）に入る前から脚を展開し、滑走状態のまま接敵しないようにする。
    constexpr float kQuadrupedEngageDistance = 28.0f;
    constexpr float kHighJumpPower = 0.38f;
    // 結合済み脚メッシュを既存EnemyRigへ再バインドしたモデル。各脚を腰ボーンへ
    // 100%固定ウェイトで割り当てているため、歩行時に部品の隙間が開かない。
    constexpr char kGroundEnemyRiggedModel[] = "EnemyR_Joined_Rig/EnemyR_Joined_Rig.gltf";
    // EnemyR は部位が別オブジェクトで出力されている。砲塔(38-48)だけを
    // 分離して独立照準させ、その他は元の形状のままメインボディとして描画する。
    constexpr char kGroundEnemyBodyModel[] = "EnemyR.obj#exclude=1-32,38-48";
    constexpr char kGroundEnemyTurretModel[] = "EnemyR.obj#groups=38-48;pivot=0.45,2.18,0.0";
    constexpr Vector3 kTurretPivot = { 0.45f, 2.18f, 0.0f };
    constexpr float kModelForwardYaw = 1.57079632679f;
    // Blender上の砲台Y回転を、glTF変換後のZ軸回転として扱う。
    // -2°が水平の写真3、-45°が砲身を持ち上げた写真2になる。
    constexpr float kTurretPitchMin = -0.78539816339f;
    constexpr float kTurretPitchMax = -0.03490658504f;
    // EnemyRの左右砲身の先端。砲塔支点からのゲーム内実寸に合わせた発射位置。
    constexpr float kTurretMuzzleForwardDistance = 0.98f;
    constexpr float kTurretMuzzleSideDistance = 0.44f;
    constexpr float kTurretMuzzleHeight = 0.12f;
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
    turretPitch_ = kTurretPitchMax;
    turretAimError_ = 0.0f;

    isDead_ = false;
    hp_ = kMaxHP;
    destructionState_ = DestructionState::Alive;
    hitStopFrames_ = 0;
    hitReactionFrames_ = 0;
    hitLean_ = 0.0f;
    skidYawVelocity_ = 0.0f;
    skidVelocity_ = { 0.0f, 0.0f, 0.0f };
    legMotionBlend_ = 0.0f;
    destructionFrames_ = 0;
    finalExplosionQueued_ = false;
    finalDestructionEffect_ = DestructionEffect::Standard;
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

void GroundEnemy::OnSpawned(const std::list<std::unique_ptr<Obstacle>>& obstacles) {
    SnapToGround(obstacles);
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

    // 2. 攻撃AI（砲塔ビーム、近接攻撃）
    // 照準対象をAIより先に更新する。これによりビーム開始フレームから砲塔は
    // 現在位置のプレイヤーへ旋回し、前フレームの位置へ向き続けない。
    turretTarget_ = playerPos;
    if (isChasingPlayer_) {
        UpdateAttackAI(playerPos, bulletManager);
    }

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

void GroundEnemy::QueueEffect(EffectEvent::Type type, const Vector3 &position, const Vector3 &direction,
    DestructionEffect destructionEffect) {
    pendingEffectEvents_.push_back({ type, position, direction, destructionEffect });
}

void GroundEnemy::TakeDamage(int damage, const Vector3 &hitDirection) {
    GroundEnemyDestructionController{}.TakeDamage(*this, damage, hitDirection);
}

void GroundEnemy::UpdateHitReaction(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    GroundEnemyDestructionController{}.UpdateHitReaction(*this, obstacles);
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
    GroundEnemyDestructionController{}.BeginDestruction(*this, hitDirection);
}

void GroundEnemy::UpdateDetachedParts() {
    GroundEnemyDestructionController{}.UpdateDetachedParts(*this);
}

void GroundEnemy::UpdateDestruction() {
    GroundEnemyDestructionController{}.UpdateDestruction(*this);
}

void GroundEnemy::UpdateDamageGlow() {
    GroundEnemyDestructionController{}.UpdateDamageGlow(*this);
}

void GroundEnemy::UpdateGroundMovement(const Vector3 &playerPos, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    GroundEnemyMovementController{}.UpdateGroundMovement(*this, playerPos, obstacles);
}

void GroundEnemy::UpdateTerrainPose(const std::list<std::unique_ptr<Obstacle>>& obstacles) {
    GroundEnemyMovementController{}.UpdateTerrainPose(*this, obstacles);
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

    // 砲塔ビーム連射中の処理
    if (attackSubState_ == GroundAttackState::Gatling) {
        // 砲塔がプレイヤーを十分に捉えるまでは発射しない。弾道だけが相手へ
        // 飛び、見た目の砲身が別方向を向く状態を防ぐ。
        if (turretAimError_ > 0.045f) {
            return;
        }
        gatlingIntervalTimer_--;
        if (gatlingIntervalTimer_ <= 0 && bulletManager) {
            // 写真2〜3で指定された動きは左右旋回ではなく、砲身を上下させる
            // ピッチ回転である。描画に使う砲身角から弾道も作り、見た目と弾を
            // 常に一致させる。
            const Vector3 bodyForward = {
                std::sin(rotation_.y), 0.0f, std::cos(rotation_.y)
            };
            const Vector3 shootDir = NormalizeOrFallback({
                bodyForward.x * std::cos(turretPitch_), -std::sin(turretPitch_),
                bodyForward.z * std::cos(turretPitch_),
            }, bodyForward);
            const Vector3 bodyRight = { std::cos(rotation_.y), 0.0f, -std::sin(rotation_.y) };
            const Vector3 turretPosition = GetLocalPartPosition(kTurretPivot);
            const Vector3 beamVelocity = { shootDir.x * 0.72f, shootDir.y * 0.72f, shootDir.z * 0.72f };

            // 左右の砲口を別々に計算する。二つのビームを同時に作るため、中央
            // から一本だけ出たり、砲身の根元から発射されたりしない。
            for (float side : { -1.0f, 1.0f }) {
                const Vector3 muzzlePos = {
                    turretPosition.x + shootDir.x * kTurretMuzzleForwardDistance + bodyRight.x * side * kTurretMuzzleSideDistance,
                    turretPosition.y + shootDir.y * kTurretMuzzleForwardDistance + kTurretMuzzleHeight,
                    turretPosition.z + shootDir.z * kTurretMuzzleForwardDistance + bodyRight.z * side * kTurretMuzzleSideDistance,
                };
                bulletManager->ShootGroundBeam(muzzlePos, beamVelocity);
            }
            gatlingShotCount_++;
            gatlingIntervalTimer_ = 24;

            if (gatlingShotCount_ >= 2) {
                attackSubState_ = GroundAttackState::Idle;
                attackTimer_ = 120;
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
        // 2. 中距離: 背部砲塔を写真2の-45°まで戻してから、写真3の水平寄り
        // （最大-2°）へ旋回させる。胴体が先に敵へ正対しても、各射撃開始時に
        // 砲台の可動が必ず見えるようにする。
        attackSubState_ = GroundAttackState::Gatling;
        gatlingShotCount_ = 0;
        gatlingIntervalTimer_ = 0;
        turretPitch_ = kTurretPitchMin;
        turretAimError_ = kTurretPitchMax - kTurretPitchMin;
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

    // Blenderで指定されたY回転は、glTFのゲーム座標では砲身の上下ピッチになる。
    // 胴体はプレイヤーへ正対したまま、背中の砲身だけを上下させて狙いを付ける。
    const Vector3 toTarget = { turretTarget_.x - position_.x, turretTarget_.y - position_.y, turretTarget_.z - position_.z };
    const float horizontalDistance = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    if (horizontalDistance > 0.001f) {
        const float targetPitch = std::clamp(
            -std::atan2(toTarget.y, horizontalDistance), kTurretPitchMin, kTurretPitchMax);
        const float pitchDifference = targetPitch - turretPitch_;
        // ビーム開始時は左端から砲身が旋回する動きを見せてから発射する。
        // 速過ぎると1フレームで目標を向いてしまい、砲台が動いていないように
        // 見えるため、発射中も約0.5秒かけて照準へ近づける。
        const float turnRate = attackSubState_ == GroundAttackState::Gatling ? 0.10f : 0.16f;
        turretPitch_ += pitchDifference * turnRate;
        turretAimError_ = std::abs(targetPitch - turretPitch_);
    }
    if (!turretDetached_) {
        AddRigJointRotation(rigSkeleton_, "Turret", { 0.0f, 0.0f, 1.0f }, turretPitch_);
    }

    ::Update(rigSkeleton_);
    object_->GetModel()->UpdateSkinCluster(object_->skinCluster, rigSkeleton_);
}

void GroundEnemy::UpdateTurretPose() {
    if (!turret_ || turretDetached_) return;

    const Vector3 toTarget = { turretTarget_.x - position_.x, turretTarget_.y - position_.y, turretTarget_.z - position_.z };
    const float horizontalDistance = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    if (horizontalDistance > 0.001f) {
        const float targetPitch = std::clamp(
            -std::atan2(toTarget.y, horizontalDistance), kTurretPitchMin, kTurretPitchMax);
        const float pitchDifference = targetPitch - turretPitch_;
        const float turnRate = attackSubState_ == GroundAttackState::Gatling ? 0.10f : 0.16f;
        turretPitch_ += pitchDifference * turnRate;
        turretAimError_ = std::abs(targetPitch - turretPitch_);
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
    turretRotation.z += turretPitch_;
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
    GroundEnemyMovementController{}.SnapToGround(*this, obstacles);
}

float GroundEnemy::GetFootOffset() const {
    return GroundEnemyMovementController{}.GetFootOffset(*this);
}

void GroundEnemy::UpdateFlightPathMovement() {
    GroundEnemyMovementController{}.UpdateFlightPathMovement(*this);
}
