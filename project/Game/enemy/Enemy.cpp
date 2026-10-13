#include "Enemy.h"
#include "EnemyActionController.h"
#include "3D/Object3dCommon.h"
#include "3D/Primitive.h"
#include "engine/Math/MyMath.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float kAttackRadius = 12.0f;
    constexpr float kBreakAwayRadius = 1.2f;
    constexpr float kRetreatStartRadius = 10.0f;
    constexpr int kRetreatCycleFrames = 300;
    constexpr int kRetreatStartFrame = 170;
    constexpr int kRetreatDurationFrames = 90;
    constexpr int kAttackInterval = 90;
    constexpr float kEnemyCruiseSpeed = 0.05f;
    constexpr float kEnemyBoostSpeed = 0.08f;
    constexpr float kEnemyTurnBlend = 0.045f;
    constexpr float kEnemyAttackTurnBlend = 0.065f;
    constexpr float kEnemyRetreatTurnBlend = 0.085f;
    constexpr float kEnemyAccelerationBlend = 0.035f;
    constexpr float kEnemyBankResponse = 0.12f;
    constexpr float kEnemyMaxBankAngle = 0.75f;
    constexpr float kEnemyBulletSpeed = 0.18f;
    constexpr float kEnemyMuzzleOffset = 1.2f;
    constexpr float kPi = 3.1415926535f;
    constexpr int kEnemyMaxHp = 2;

    float LengthSq(const Vector3 &v) {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    float Length(const Vector3 &v) {
        return std::sqrt(LengthSq(v));
    }

    Vector3 Add(const Vector3 &a, const Vector3 &b) {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    Vector3 Subtract(const Vector3 &a, const Vector3 &b) {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    Vector3 Scale(const Vector3 &v, float scalar) {
        return { v.x * scalar, v.y * scalar, v.z * scalar };
    }

    float Dot(const Vector3 &a, const Vector3 &b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    float Abs(float value) {
        return value < 0.0f ? -value : value;
    }

    Vector3 AbsVector(const Vector3 &value) {
        return { Abs(value.x), Abs(value.y), Abs(value.z) };
    }

    float ProjectionRadius(const OBB &obb, const Vector3 &axis) {
        return obb.size.x * Abs(Dot(obb.orientations[0], axis)) +
            obb.size.y * Abs(Dot(obb.orientations[1], axis)) +
            obb.size.z * Abs(Dot(obb.orientations[2], axis));
    }

    float GetAxisSize(const Vector3 &value, int axisIndex) {
        if (axisIndex == 0) return value.x;
        if (axisIndex == 1) return value.y;
        return value.z;
    }

    Vector3 ComposeFromAxes(const OBB &basis, const Vector3 &amount) {
        return Add(Add(Scale(basis.orientations[0], amount.x), Scale(basis.orientations[1], amount.y)), Scale(basis.orientations[2], amount.z));
    }

    Vector3 NormalizeOr(const Vector3 &v, const Vector3 &fallback) {
        float length = Length(v);
        if (length <= 0.001f) {
            return fallback;
        }
        return Scale(v, 1.0f / length);
    }

    Vector3 ForwardFromRotation(const Vector3 &rotation) {
        Matrix4x4 rotMat = MyMath::Multiply(
            MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation.x), MyMath::MakeRotateYMatrix(rotation.y)),
            MyMath::MakeRotateZMatrix(rotation.z));
        return NormalizeOr(MyMath::Transform({ 0.0f, 0.0f, 1.0f }, rotMat), { 0.0f, 0.0f, 1.0f });
    }

    Vector3 ToEulerRotation(const Vector3 &forward, float bankAngle) {
        Vector3 normalizedForward = NormalizeOr(forward, { 0.0f, 0.0f, 1.0f });
        float clampedY = std::clamp(normalizedForward.y, -1.0f, 1.0f);
        return {
            -std::asin(clampedY),
            std::atan2(normalizedForward.x, normalizedForward.z),
            bankAngle
        };
    }

    size_t ResolvePathIndex(int index, size_t pointCount, bool loop) {
        if (pointCount == 0) {
            return 0;
        }

        if (loop) {
            int count = static_cast<int>(pointCount);
            int wrapped = index % count;
            if (wrapped < 0) {
                wrapped += count;
            }
            return static_cast<size_t>(wrapped);
        }

        int last = static_cast<int>(pointCount - 1);
        return static_cast<size_t>(std::clamp(index, 0, last));
    }

    Vector3 CatmullRom(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;

        Vector3 result = Add(
            Add(
                Scale(p1, 2.0f),
                Scale(Subtract(p2, p0), t)),
            Add(
                Scale(Add(Scale(p0, 2.0f), Add(Scale(p1, -5.0f), Add(Scale(p2, 4.0f), Scale(p3, -1.0f)))), t2),
                Scale(Add(Scale(p0, -1.0f), Add(Scale(p1, 3.0f), Add(Scale(p2, -3.0f), p3))), t3)));

        return Scale(result, 0.5f);
    }

    Vector3 EvaluateFlightPath(const std::vector<Vector3> &points, size_t segmentIndex, float t, bool loop) {
        int baseIndex = static_cast<int>(segmentIndex);
        const Vector3 &p0 = points[ResolvePathIndex(baseIndex - 1, points.size(), loop)];
        const Vector3 &p1 = points[ResolvePathIndex(baseIndex, points.size(), loop)];
        const Vector3 &p2 = points[ResolvePathIndex(baseIndex + 1, points.size(), loop)];
        const Vector3 &p3 = points[ResolvePathIndex(baseIndex + 2, points.size(), loop)];
        return CatmullRom(p0, p1, p2, p3, t);
    }

    int GetFlightSide(size_t spawnPointIndex, const Vector3 &position) {
        if (spawnPointIndex != Enemy::kNoSpawnPoint) {
            return (spawnPointIndex % 2 == 0) ? 1 : -1;
        }
        return (position.x >= 0.0f) ? 1 : -1;
    }

    int GetRetreatPhaseOffset(size_t spawnPointIndex, const Vector3 &position) {
        if (spawnPointIndex != Enemy::kNoSpawnPoint) {
            return static_cast<int>((spawnPointIndex * 73) % kRetreatCycleFrames);
        }
        return static_cast<int>(std::abs(position.x) * 37.0f) % kRetreatCycleFrames;
    }

    void ShootAtTarget(EnemyBulletManager *bulletManager, const Vector3 &shooterPosition, const Vector3 &targetPosition, const Vector3 &fallbackDirection) {
        if (!bulletManager) {
            return;
        }

        const Vector3 toTarget = Subtract(targetPosition, shooterPosition);
        const float distanceToTarget = Length(toTarget);
        const Vector3 directionToTarget = NormalizeOr(toTarget, fallbackDirection);
        const float muzzleOffset = (std::min)(kEnemyMuzzleOffset, distanceToTarget * 0.5f);
        const Vector3 bulletPosition = Add(shooterPosition, Scale(directionToTarget, muzzleOffset));
        const Vector3 shootDirection = NormalizeOr(Subtract(targetPosition, bulletPosition), directionToTarget);
        const Vector3 bulletVelocity = Scale(shootDirection, kEnemyBulletSpeed);
        bulletManager->Shoot(bulletPosition, bulletVelocity);
    }
}

void Enemy::Initialize(const Vector3 &position) {
    // 再初期化時は既存のGPUバッファを再利用する。
    if (!object_) {
        object_ = std::make_unique<Object3d>();
        object_->Initialize(Object3dCommon::GetInstance());
    }

    // 自機と同じBoxモデルを使い回し
    object_->SetModel("EnemyBox");

    // 敵らしく赤色にする
    if (object_->GetModel()) {
        object_->GetModel()->SetColor({ 0.8f, 0.1f, 0.1f, 1.0f });
    }

    // プレイヤーと同じ大きさにする
    scale_ = { 0.2f, 0.2f, 0.2f };
    object_->SetScale(scale_);

    position_ = position;
    rotation_ = { 0.0f, 0.0f, 0.0f };
    forward_ = ForwardFromRotation(rotation_);
    currentSpeed_ = kEnemyCruiseSpeed;
    velocity_ = Scale(forward_, currentSpeed_);
    bankAngle_ = 0.0f;
    flightTimer_ = 0;
    isDead_ = false;
    state_ = EnemyState::Approach;
    attackTimer_ = kAttackInterval;
    flightPathPoints_.clear();
    hasFlightPath_ = false;
    flightPathLoop_ = false;
    flightPathSpeed_ = kEnemyCruiseSpeed;
    flightPathSegmentIndex_ = 0;
    flightPathSegmentT_ = 0.0f;
    isChasingPlayer_ = true;
    hp_ = kEnemyMaxHp;

    // リスポーン直後の描画でもBlenderで設定した地点に表示されるように同期する。
    object_->SetTranslate(position_);
    object_->SetRotate(rotation_);
    object_->Update();
}

void Enemy::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {

    if (isDead_) return; // 死んでいたら何もしない

    // 1. AI思考と移動
    UpdateAI(playerPos, bulletManager);

    // 2. 当たり判定（押し出し）
    CheckCollision(obstacles);

    // 今は動かないので、座標をセットして更新するだけ
    UpdateModel();
}

void Enemy::UpdateModel() {
    if (object_) {
        object_->SetTranslate(position_);
        object_->SetRotate(rotation_);
        object_->SetScale(scale_);
        object_->Update();
    }

    // Object3d::Update() calculates the WVP matrix using the current camera.
    // Path markers do not move, but their WVP must still be refreshed whenever
    // the camera moves or switches (for example, to the debug fly camera).
    if (pathVisualizationEnabled_) {
        if (pathVisualizers_.empty() && hasFlightPath_) {
            BuildPathVisualizers();
        }
        for (const auto &visualizer : pathVisualizers_) {
            if (visualizer) {
                visualizer->Update();
            }
        }
    }
}

void Enemy::Draw() {
    if (pathVisualizationEnabled_) {
        for (const auto &visualizer : pathVisualizers_) {
            if (visualizer) {
                visualizer->Draw();
            }
        }
    }

    if (object_) {
        object_->Draw();
    }
}

Vector3 Enemy::GetWorldHalfExtents() const {
    if (!object_ || !object_->GetModel()) {
        return AbsVector(scale_);
    }

    const Vector3 localHalf = object_->GetModel()->GetHalfExtents();
    const Vector3 absScale = AbsVector(scale_);
    return { localHalf.x * absScale.x, localHalf.y * absScale.y, localHalf.z * absScale.z };
}

float Enemy::GetCollisionRadius() const {
    const Vector3 halfExtents = GetWorldHalfExtents();
    float radius = halfExtents.x;
    if (halfExtents.y > radius) radius = halfExtents.y;
    if (halfExtents.z > radius) radius = halfExtents.z;
    return radius;
}

OBB Enemy::GetOBB() const {
    OBB obb{};
    obb.center = position_;
    obb.size = GetWorldHalfExtents();

    const Matrix4x4 rotationMatrix = MyMath::Multiply(
        MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::MakeRotateYMatrix(rotation_.y)),
        MyMath::MakeRotateZMatrix(rotation_.z));
    obb.orientations[0] = MyMath::Normalize(Vector3{ rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2] });
    obb.orientations[1] = MyMath::Normalize(Vector3{ rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2] });
    obb.orientations[2] = MyMath::Normalize(Vector3{ rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2] });

    if (object_ && object_->GetModel()) {
        const Vector3 localCenter = object_->GetModel()->GetBoundsCenter();
        const Vector3 scaledCenter = { localCenter.x * scale_.x, localCenter.y * scale_.y, localCenter.z * scale_.z };
        const Vector3 rotatedCenter = MyMath::Transform(scaledCenter, rotationMatrix);
        obb.center = Add(position_, rotatedCenter);
    }

    return obb;
}

// 当たった時の処理
void Enemy::OnCollision() {
    EnemyActionController{}.Destroy(*this);
}

void Enemy::TakeDamage(int damage) {
    EnemyActionController{}.TakeDamage(*this, damage);
}

void Enemy::StartChasingPlayer() {
    EnemyActionController{}.StartChasing(*this);
}

void Enemy::SetRotation(const Vector3 &rotation) {
    rotation_ = rotation;
    bankAngle_ = rotation.z;
    forward_ = ForwardFromRotation(rotation_);
    currentSpeed_ = (currentSpeed_ > 0.0f) ? currentSpeed_ : kEnemyCruiseSpeed;
    velocity_ = Scale(forward_, currentSpeed_);

    if (object_) {
        object_->SetRotate(rotation_);
        object_->Update();
    }
}

void Enemy::SetFlightPath(const std::vector<Vector3> &points, bool loop, float speed) {
    flightPathPoints_ = points;
    hasFlightPath_ = flightPathPoints_.size() >= 2;
    flightPathLoop_ = loop;
    flightPathSpeed_ = (std::isfinite(speed) && speed > 0.0f) ? speed : kEnemyCruiseSpeed;
    flightPathSegmentIndex_ = 0;
    flightPathSegmentT_ = 0.0f;
    isChasingPlayer_ = true;
    
    pathVisualizers_.clear();

    if (!hasFlightPath_) {
        return;
    }

    position_ = flightPathPoints_[0];
    forward_ = NormalizeOr(Subtract(flightPathPoints_[1], flightPathPoints_[0]), forward_);
    currentSpeed_ = flightPathSpeed_;
    velocity_ = Scale(forward_, currentSpeed_);
    bankAngle_ = 0.0f;
    rotation_ = ToEulerRotation(forward_, bankAngle_);

    if (object_) {
        object_->SetTranslate(position_);
        object_->SetRotate(rotation_);
        object_->Update();
    }
    
    if (pathVisualizationEnabled_) {
        BuildPathVisualizers();
    }
}

void Enemy::BuildPathVisualizers() {
    pathVisualizers_.clear();
    if (!hasFlightPath_ || flightPathPoints_.size() < 2) {
        return;
    }

    // デバッグ表示が要求された時だけ経路球のGPUリソースを生成する。
    const size_t segmentCount = flightPathLoop_
        ? flightPathPoints_.size()
        : flightPathPoints_.size() - 1;
    if (segmentCount > 0) {
        constexpr int dotsPerSegment = 10;
        pathVisualizers_.reserve(segmentCount * dotsPerSegment);
        for (size_t i = 0; i < segmentCount; ++i) {
            for (int j = 0; j < dotsPerSegment; ++j) {
                float t = static_cast<float>(j) / static_cast<float>(dotsPerSegment);
                Vector3 pt = EvaluateFlightPath(flightPathPoints_, i, t, flightPathLoop_);
                
                auto sphere = std::make_unique<Primitive>();
                sphere->Initialize(Object3dCommon::GetInstance(), PrimitiveType::Sphere);
                sphere->SetScale({ 0.15f, 0.15f, 0.15f });
                if (sphere->GetModel()) {
                    sphere->GetModel()->SetColor({ 1.0f, 0.5f, 0.0f, 0.6f }); // オレンジ系の半透明
                }
                sphere->SetTranslate(pt);
                sphere->Update();
                pathVisualizers_.push_back(std::move(sphere));
            }
        }
    }
}

void Enemy::UpdateAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    flightTimer_++;

    if (hasFlightPath_ && !isChasingPlayer_) {
        UpdateFlightPathAI(playerPos, bulletManager);
        return;
    }

    Vector3 toPlayer = Subtract(playerPos, position_);
    float distSqToPlayer = LengthSq(toPlayer);
    float distToPlayer = std::sqrt(distSqToPlayer);
    Vector3 directionToPlayer = NormalizeOr(toPlayer, forward_);

    bool isInAttackRange = distSqToPlayer <= kAttackRadius * kAttackRadius;
    state_ = isInAttackRange ? EnemyState::Attack : EnemyState::Approach;

    int flightSide = GetFlightSide(spawnPointIndex_, position_);
    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
    Vector3 sideDirection = MyMath::Cross(worldUp, directionToPlayer);
    if (LengthSq(sideDirection) <= 0.001f) {
        sideDirection = { static_cast<float>(flightSide), 0.0f, 0.0f };
    } else {
        sideDirection = Scale(NormalizeOr(sideDirection, { 1.0f, 0.0f, 0.0f }), static_cast<float>(flightSide));
    }

    int retreatPhase = (flightTimer_ + GetRetreatPhaseOffset(spawnPointIndex_, position_)) % kRetreatCycleFrames;
    bool isRetreating = isInAttackRange &&
        distSqToPlayer <= kRetreatStartRadius * kRetreatStartRadius &&
        retreatPhase >= kRetreatStartFrame &&
        retreatPhase < kRetreatStartFrame + kRetreatDurationFrames;

    float wave = std::sin(static_cast<float>(flightTimer_) * 0.035f + static_cast<float>(flightSide) * kPi * 0.5f);
    float desiredAltitude = playerPos.y + 7.0f + wave * 4.0f;
    float altitudeCorrection = std::clamp((desiredAltitude - position_.y) / 45.0f, -0.35f, 0.35f);

    Vector3 desiredDirection = directionToPlayer;
    float desiredSpeed = kEnemyBoostSpeed;
    float turnBlend = kEnemyTurnBlend;

    if (isRetreating) {
        desiredDirection = Add(
            Scale(directionToPlayer, -1.0f),
            Scale(sideDirection, 0.55f));
        desiredDirection.y += altitudeCorrection * 0.8f;

        desiredSpeed = kEnemyBoostSpeed;
        turnBlend = kEnemyRetreatTurnBlend;
    } else if (isInAttackRange) {
        float tangentWeight = (distToPlayer < kBreakAwayRadius) ? 1.25f : 0.75f;
        float noseWeight = (distToPlayer < kBreakAwayRadius) ? -0.15f : 0.65f;

        desiredDirection = Add(
            Scale(directionToPlayer, noseWeight),
            Scale(sideDirection, tangentWeight));
        desiredDirection.y += altitudeCorrection;

        desiredSpeed = (distToPlayer < kBreakAwayRadius) ? kEnemyBoostSpeed : kEnemyCruiseSpeed;
        turnBlend = kEnemyAttackTurnBlend;
    } else {
        desiredDirection.y += altitudeCorrection * 0.5f;
    }

    desiredDirection = NormalizeOr(desiredDirection, directionToPlayer);

    Vector3 oldForward = forward_;
    forward_ = NormalizeOr(Add(Scale(forward_, 1.0f - turnBlend), Scale(desiredDirection, turnBlend)), oldForward);

    float targetSpeed = desiredSpeed;
    currentSpeed_ += (targetSpeed - currentSpeed_) * kEnemyAccelerationBlend;
    velocity_ = Scale(forward_, currentSpeed_);

    position_.x += velocity_.x;
    position_.y += velocity_.y;
    position_.z += velocity_.z;

    float yawTurn = MyMath::Dot(MyMath::Cross(oldForward, forward_), worldUp);
    float targetBankAngle = std::clamp(-yawTurn * 18.0f, -kEnemyMaxBankAngle, kEnemyMaxBankAngle);
    bankAngle_ += (targetBankAngle - bankAngle_) * kEnemyBankResponse;
    rotation_ = ToEulerRotation(forward_, bankAngle_);

    attackTimer_++;
    if (!isRetreating && isInAttackRange && attackTimer_ >= kAttackInterval && bulletManager) {
        ShootAtTarget(bulletManager, position_, playerPos, directionToPlayer);
        attackTimer_ = 0;
    }
}

void Enemy::UpdateFlightPathAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    if (flightPathPoints_.size() < 2) {
        hasFlightPath_ = false;
        return;
    }

    const size_t segmentCount = flightPathLoop_ ? flightPathPoints_.size() : flightPathPoints_.size() - 1;
    if (segmentCount == 0) {
        return;
    }

    Vector3 oldPosition = position_;
    Vector3 oldForward = forward_;

    float remainingDistance = flightPathSpeed_;
    while (remainingDistance > 0.0f) {
        size_t nextIndex = ResolvePathIndex(static_cast<int>(flightPathSegmentIndex_ + 1), flightPathPoints_.size(), flightPathLoop_);
        Vector3 segmentVector = Subtract(flightPathPoints_[nextIndex], flightPathPoints_[flightPathSegmentIndex_]);
        float segmentLength = Length(segmentVector);
        
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

    position_ = EvaluateFlightPath(flightPathPoints_, flightPathSegmentIndex_, flightPathSegmentT_, flightPathLoop_);
    Vector3 pathForward = NormalizeOr(Subtract(position_, oldPosition), oldForward);
    forward_ = pathForward;
    currentSpeed_ = flightPathSpeed_;
    velocity_ = Scale(forward_, currentSpeed_);

    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
    float yawTurn = MyMath::Dot(MyMath::Cross(oldForward, forward_), worldUp);
    float targetBankAngle = std::clamp(-yawTurn * 18.0f, -kEnemyMaxBankAngle, kEnemyMaxBankAngle);
    bankAngle_ += (targetBankAngle - bankAngle_) * kEnemyBankResponse;
    rotation_ = ToEulerRotation(forward_, bankAngle_);

    Vector3 toPlayer = Subtract(playerPos, position_);
    float distSqToPlayer = LengthSq(toPlayer);
    Vector3 directionToPlayer = NormalizeOr(toPlayer, forward_);
    bool isInAttackRange = distSqToPlayer <= kAttackRadius * kAttackRadius;
    state_ = isInAttackRange ? EnemyState::Attack : EnemyState::Approach;

    attackTimer_++;
    if (isInAttackRange && attackTimer_ >= kAttackInterval && bulletManager) {
        ShootAtTarget(bulletManager, position_, playerPos, directionToPlayer);
        attackTimer_ = 0;
    }
}

void Enemy::CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    // =========================================================
    //    敵と障害物の当たり判定
    // =========================================================
    OBB enemyOBB = GetOBB();
    for (const auto &obstacle : obstacles) {
        if (!obstacle) {
            continue;
        }

        // 地形は巨大なOBBではなく三角形メッシュで接地・衝突を処理する。
        // ここで地形全体の箱に対して再度押し出すと、地表へ補正した地上敵を
        // 箱の裏側（地面の下）まで移動させ、ロックオン表示だけが残る原因になる。
        // StageBounds はメッシュ設定に関係なく範囲制限として処理を継続する。
        if (!obstacle->IsStageBounds() && obstacle->IsUseMeshCollider()) {
            continue;
        }

        const OBB obsOBB = obstacle->GetOBB();
        const Vector3 diff = {
            enemyOBB.center.x - obsOBB.center.x,
            enemyOBB.center.y - obsOBB.center.y,
            enemyOBB.center.z - obsOBB.center.z,
        };
        const float localDistance[3] = {
            Dot(diff, obsOBB.orientations[0]),
            Dot(diff, obsOBB.orientations[1]),
            Dot(diff, obsOBB.orientations[2]),
        };
        const float enemyProjection[3] = {
            ProjectionRadius(enemyOBB, obsOBB.orientations[0]),
            ProjectionRadius(enemyOBB, obsOBB.orientations[1]),
            ProjectionRadius(enemyOBB, obsOBB.orientations[2]),
        };

        // StageBounds は壁ではなくステージ全体の範囲なので、内側に留める。
        if (obstacle->IsStageBounds()) {
            Vector3 localPushOut = { 0.0f, 0.0f, 0.0f };

            for (int axis = 0; axis < 3; ++axis) {
                const float limit = (std::max)(0.0f, GetAxisSize(obsOBB.size, axis) - enemyProjection[axis]);
                if (localDistance[axis] > limit) {
                    if (axis == 0) localPushOut.x = limit - localDistance[axis];
                    if (axis == 1) localPushOut.y = limit - localDistance[axis];
                    if (axis == 2) localPushOut.z = limit - localDistance[axis];
                } else if (localDistance[axis] < -limit) {
                    if (axis == 0) localPushOut.x = -limit - localDistance[axis];
                    if (axis == 1) localPushOut.y = -limit - localDistance[axis];
                    if (axis == 2) localPushOut.z = -limit - localDistance[axis];
                }
            }

            Vector3 worldPushOut = ComposeFromAxes(obsOBB, localPushOut);
            position_.x += worldPushOut.x;
            position_.y += worldPushOut.y;
            position_.z += worldPushOut.z;
            enemyOBB.center = Add(enemyOBB.center, worldPushOut);
            continue;
        }

        if (!MyMath::IsCollision(enemyOBB, obsOBB)) {
            continue;
        }

        float overlapX = obsOBB.size.x + enemyProjection[0] - std::abs(localDistance[0]);
        float overlapY = obsOBB.size.y + enemyProjection[1] - std::abs(localDistance[1]);
        float overlapZ = obsOBB.size.z + enemyProjection[2] - std::abs(localDistance[2]);

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
            position_.x += worldPushOut.x;
            position_.y += worldPushOut.y;
            position_.z += worldPushOut.z;
            enemyOBB.center = Add(enemyOBB.center, worldPushOut);
        }
    }
}


