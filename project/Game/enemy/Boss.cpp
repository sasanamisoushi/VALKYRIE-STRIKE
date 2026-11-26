#include "Boss.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/EnemyBulletManager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace {
Vector3 AddVector(const Vector3 &a, const Vector3 &b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 SubtractVector(const Vector3 &a, const Vector3 &b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 ScaleVector(const Vector3 &v, float s) { return { v.x * s, v.y * s, v.z * s }; }
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

// Boss1 の全長約8.5mを、プレイヤーを大きく上回る戦艦サイズまで拡大する。
constexpr Vector3 kBossScale = { 1.85f, 1.55f, 1.85f };
constexpr float kBossSidewaysYaw = 1.57079632679f; // 90 degrees

// Blender で選択されている平面を、OBJ出力・X反転後のゲーム座標へ換算した値。
// 平面と同じ高さ・横幅で、中央が外側へ張り出す円弧状のシールドを作る。
constexpr std::array<Vector3, 2> kSideShieldPlaneCenters = {
    // Blender で選択された平面側と、反対側にある同じ装甲板側。
    Vector3{ 2.5535f, 3.5352f, 0.0243f },
    Vector3{ -2.5535f, 3.5352f, 0.0243f },
};
constexpr float kSideShieldGeneratorLockOnRadius = 3.85f;

// OBJ内の実オブジェクト順を基準にする。側面の長い板（24/25, 28）は
// シールド発生装置として別Object3dで管理するため船体から除外する。
// 24 と 25 は同位置に重複しているため、24だけを発生装置モデルとして使用する。
constexpr char kBossBodyModel[] = "Boss1/Boss1.obj#exclude=0,2-13,19-20,22,24-25,28-30";
// 通常時は上記の船体モデルを一体で描画する。撃破時だけは、砲台以外の装甲・甲板も
// 飛ばせるよう、同じOBJグループを重複なしの8ブロックへ分割して使用する。
constexpr std::array<const char*, 8> kCinematicHullDebrisModels = {
    "Boss1/Boss1.obj#groups=1,14-15",
    "Boss1/Boss1.obj#groups=16-18",
    "Boss1/Boss1.obj#groups=21,23-24",
    "Boss1/Boss1.obj#groups=25-28",
    "Boss1/Boss1.obj#groups=31-34",
    "Boss1/Boss1.obj#groups=35-39",
    "Boss1/Boss1.obj#groups=40-44",
    "Boss1/Boss1.obj#groups=45-54",
};
constexpr std::array<const char*, 3> kMainTurretModels = {
    // 左右の大きな箱（23/27）は後部装甲の一部なので船体に固定する。
    // 回転させるのは砲身だけにし、スラスター周辺の甲板を引き剥がさない。
    "Boss1/Boss1.obj#groups=4-5;pivot=-1.73,3.85,-2.62",
    "Boss1/Boss1.obj#groups=8-9;pivot=1.83,3.85,-2.62",
    "Boss1/Boss1.obj#groups=12-13;pivot=0.04,2.38,2.26",
};
constexpr char kMachineGunTurretModel[] = "Boss1/Boss1.obj#groups=6-7,22;pivot=0.01,4.70,0.90";
constexpr std::array<const char*, 2> kMiniTurretBaseModels = {
    "Boss1/Boss1.obj#groups=30;pivot=-0.59,4.30,-0.76",
    "Boss1/Boss1.obj#groups=29;pivot=0.58,4.30,-0.76",
};
constexpr std::array<const char*, 2> kMiniTurretBarrelModels = {
    "Boss1/Boss1.obj#groups=11;pivot=-0.72,4.34,-0.78",
    "Boss1/Boss1.obj#groups=10;pivot=0.70,4.34,-0.78",
};

// 艦首に見えている三基。中央上部の砲座は箱形状も含め、左右は独立した砲身を
// 砲台として分離する。どれも砲身の先端から通常弾を発射する。
constexpr std::array<const char*, 3> kForwardTurretModels = {
    "Boss1/Boss1.obj#groups=0,19-20;pivot=0.02,4.42,3.23",
    "Boss1/Boss1.obj#groups=2;pivot=-1.31,3.28,3.57",
    "Boss1/Boss1.obj#groups=3;pivot=1.33,3.28,3.57",
};

constexpr int kTurretMaxHP = 12;

// ロックオン、被弾判定、描画・発射停止で共有する砲台番号。
constexpr size_t kForwardTurretFirst = 0;
constexpr size_t kMainTurretFirst = 3;
constexpr size_t kMiniTurretFirst = 6;
constexpr size_t kMachineGunTurretIndex = 8;

// 艦尾の中央・左・右パネルにある2列×2段の穴の中心。
// 順番は中央4基、左パネル4基、右パネル4基。どの炎も枠の内側から出る。
constexpr std::array<Vector3, 12> kRearThrusterPanelCenters = {
    // 中央パネル
    Vector3{ -0.45f, 3.12f, -4.68f }, Vector3{  0.45f, 3.12f, -4.68f },
    Vector3{ -0.45f, 1.55f, -4.34f }, Vector3{  0.45f, 1.55f, -4.34f },
    // 左パネル
    Vector3{ -2.09f, 3.55f, -4.10f }, Vector3{ -1.47f, 3.55f, -4.10f },
    Vector3{ -2.09f, 3.05f, -3.90f }, Vector3{ -1.47f, 3.05f, -3.90f },
    // 右パネル
    Vector3{  1.40f, 3.55f, -4.10f }, Vector3{  2.03f, 3.55f, -4.10f },
    Vector3{  1.40f, 3.05f, -3.90f }, Vector3{  2.03f, 3.05f, -3.90f },
};
constexpr Vector3 kRearThrusterDirection = { 0.0f, 0.0f, -1.0f };

float WrapAngle(float angle) {
    constexpr float kPi = 3.14159265359f;
    while (angle > kPi) angle -= kPi * 2.0f;
    while (angle < -kPi) angle += kPi * 2.0f;
    return angle;
}

Vector3 RotatePitchYaw(const Vector3& value, float pitch, float yaw) {
    const float pitchCos = std::cos(pitch);
    const float pitchSin = std::sin(pitch);
    const Vector3 pitched = {
        value.x,
        value.y * pitchCos - value.z * pitchSin,
        value.y * pitchSin + value.z * pitchCos,
    };
    const float yawCos = std::cos(yaw);
    const float yawSin = std::sin(yaw);
    return {
        pitched.x * yawCos + pitched.z * yawSin,
        pitched.y,
        -pitched.x * yawSin + pitched.z * yawCos,
    };
}
}

void Boss::Initialize(const Vector3 &position) {
    Enemy::Initialize(position);
    cinematicWreckPresentation_ = false;
    cinematicDebrisActive_ = false;
    cinematicDebrisParts_.fill(nullptr);
    cinematicDebrisVelocities_.fill({});
    cinematicDebrisAngularVelocities_.fill({});
    cinematicSmallDebrisVelocities_.fill({});
    cinematicSmallDebrisAngularVelocities_.fill({});

    // Boss1 の船体と可動砲塔を別モデルとして読み込む。
    ModelManager *modelManager = ModelManager::GetInstance();
    if (!modelManager->FindModel(kBossBodyModel)) {
        modelManager->LoadModel(kBossBodyModel);
    }
    if (Object3d* obj = GetObject3d()) {
        obj->SetModel(kBossBodyModel);
        if (obj->GetModel()) {
            obj->GetModel()->SetColor({ 0.20f, 0.33f, 0.39f, 1.0f });
            obj->GetModel()->SetShininess(96.0f);
        }
    }
    if (!modelManager->FindModel("BossSmallDebris")) {
        modelManager->CreateBoxModel("BossSmallDebris");
    }
    if (Model* smallDebrisModel = modelManager->FindModel("BossSmallDebris")) {
        smallDebrisModel->SetColor({ 0.08f, 0.08f, 0.09f, 1.0f });
        smallDebrisModel->SetShininess(64.0f);
    }
    for (size_t index = 0; index < cinematicSmallDebris_.size(); ++index) {
        cinematicSmallDebris_[index] = std::make_unique<Object3d>();
        cinematicSmallDebris_[index]->Initialize(Object3dCommon::GetInstance());
        cinematicSmallDebris_[index]->SetModel("BossSmallDebris");
    }
    for (size_t index = 0; index < cinematicHullDebris_.size(); ++index) {
        const char* hullModel = kCinematicHullDebrisModels[index];
        if (!modelManager->FindModel(hullModel)) {
            modelManager->LoadModel(hullModel);
        }
        cinematicHullDebris_[index] = std::make_unique<Object3d>();
        cinematicHullDebris_[index]->Initialize(Object3dCommon::GetInstance());
        cinematicHullDebris_[index]->SetModel(hullModel);
        if (cinematicHullDebris_[index]->GetModel()) {
            cinematicHullDebris_[index]->GetModel()->SetColor({ 0.20f, 0.33f, 0.39f, 1.0f });
            cinematicHullDebris_[index]->GetModel()->SetShininess(96.0f);
        }
    }
    for (size_t index = 0; index < mainTurrets_.size(); ++index) {
        const char* turretModel = kMainTurretModels[index];
        if (!modelManager->FindModel(turretModel)) {
            modelManager->LoadModel(turretModel);
        }
        mainTurrets_[index] = std::make_unique<Object3d>();
        mainTurrets_[index]->Initialize(Object3dCommon::GetInstance());
        mainTurrets_[index]->SetModel(turretModel);
        if (mainTurrets_[index]->GetModel()) {
            mainTurrets_[index]->GetModel()->SetColor({ 0.29f, 0.43f, 0.48f, 1.0f });
            mainTurrets_[index]->GetModel()->SetShininess(128.0f);
        }
    }
    for (size_t index = 0; index < miniTurretBases_.size(); ++index) {
        const char* baseModel = kMiniTurretBaseModels[index];
        if (!modelManager->FindModel(baseModel)) {
            modelManager->LoadModel(baseModel);
        }
        miniTurretBases_[index] = std::make_unique<Object3d>();
        miniTurretBases_[index]->Initialize(Object3dCommon::GetInstance());
        miniTurretBases_[index]->SetModel(baseModel);
        if (miniTurretBases_[index]->GetModel()) {
            miniTurretBases_[index]->GetModel()->SetColor({ 0.29f, 0.43f, 0.48f, 1.0f });
            miniTurretBases_[index]->GetModel()->SetShininess(128.0f);
        }
    }
    for (size_t index = 0; index < miniTurretBarrels_.size(); ++index) {
        const char* barrelModel = kMiniTurretBarrelModels[index];
        if (!modelManager->FindModel(barrelModel)) {
            modelManager->LoadModel(barrelModel);
        }
        miniTurretBarrels_[index] = std::make_unique<Object3d>();
        miniTurretBarrels_[index]->Initialize(Object3dCommon::GetInstance());
        miniTurretBarrels_[index]->SetModel(barrelModel);
        if (miniTurretBarrels_[index]->GetModel()) {
            miniTurretBarrels_[index]->GetModel()->SetColor({ 0.78f, 0.48f, 0.16f, 1.0f });
            miniTurretBarrels_[index]->GetModel()->SetShininess(160.0f);
        }
    }
    if (!modelManager->FindModel(kMachineGunTurretModel)) {
        modelManager->LoadModel(kMachineGunTurretModel);
    }
    machineGunTurret_ = std::make_unique<Object3d>();
    machineGunTurret_->Initialize(Object3dCommon::GetInstance());
    machineGunTurret_->SetModel(kMachineGunTurretModel);
    if (machineGunTurret_->GetModel()) {
        machineGunTurret_->GetModel()->SetColor({ 0.68f, 0.36f, 0.10f, 1.0f });
        machineGunTurret_->GetModel()->SetShininess(144.0f);
    }
    for (size_t index = 0; index < forwardTurrets_.size(); ++index) {
        const char* turretModel = kForwardTurretModels[index];
        if (!modelManager->FindModel(turretModel)) {
            modelManager->LoadModel(turretModel);
        }
        forwardTurrets_[index] = std::make_unique<Object3d>();
        forwardTurrets_[index]->Initialize(Object3dCommon::GetInstance());
        forwardTurrets_[index]->SetModel(turretModel);
        if (forwardTurrets_[index]->GetModel()) {
            forwardTurrets_[index]->GetModel()->SetColor({ 0.84f, 0.42f, 0.12f, 1.0f });
            forwardTurrets_[index]->GetModel()->SetShininess(160.0f);
        }
    }
    InitializeSideShields();
    InitializeRearThrusters();

    SetIsBoss(true);
    // 本体と砲台が同じ戦艦スケールを使用するよう、描画だけでなく当たり判定の
    // 基準になる Enemy::scale_ も同期する。
    SetScale(kBossScale);
    SetHP(tuning_.maxHP);
    SetScale(kBossScale);
    // 艦首をプレイヤーの進行方向に対して横へ向ける。
    SetRotation({ 0.0f, kBossSidewaysYaw, 0.0f });
    actionTimer_ = 0;
    summonRequests_ = 0;
    turretYaw_ = 0.0f;
    turretPitch_ = 0.0f;
    mainTurretYaws_.fill(0.0f);
    mainTurretEngaged_.fill(false);
    miniTurretYaws_.fill(0.0f);
    miniTurretPitches_.fill(0.0f);
    turretHP_.fill(kTurretMaxHP);
    turretDestroyed_.fill(false);
    sideShieldGeneratorHP_.fill(kSideShieldGeneratorMaxHP);
    sideShieldGeneratorDestroyed_.fill(false);
    sideShieldState_ = SideShieldState::Charging;
    sideShieldStateTimer_ = 0;
    sideShieldDeployProgress_ = 0.0f;
    sideShieldBuiltProgress_ = -1.0f;
    UpdateRearThrusters();
    UpdateModel();
}

void Boss::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    if (IsDead()) return;

    const Vector3 prevPos = GetPosition();
    ++actionTimer_;
    MoveBy({ 0.0f, std::sin(static_cast<float>(actionTimer_) * 0.012f) * 0.008f, 0.0f });
    UpdateTurretAim(playerPos);
    // 以前の4秒周期では同時弾数が多過ぎたため、5秒周期へ広げて各砲の間隔も空ける。
    const int cycle = actionTimer_ % 300;
    if (cycle >= 25 && cycle < 85 && cycle % 10 == 0) FireMachineGun(playerPos, bulletManager);
    if (cycle == 110 || cycle == 126) FireForwardTurret(playerPos, bulletManager, 0);
    if (cycle == 116 || cycle == 132 || cycle == 148) {
        FireForwardTurret(playerPos, bulletManager, 1);
        FireForwardTurret(playerPos, bulletManager, 2);
    }
    if (cycle == 170 && mainTurretEngaged_[0]) FireMainBeam(playerPos, bulletManager, 0);
    if (cycle == 200 && mainTurretEngaged_[1]) FireMainBeam(playerPos, bulletManager, 1);
    if (cycle == 230 && mainTurretEngaged_[2]) FireMainBeam(playerPos, bulletManager, 2);
    // 上部ミニ連装砲は短い4連射だけにして、回避可能な弾幕密度にする。
    if (cycle >= 250 && cycle < 295 && (cycle - 250) % 12 == 0) FireMainBeam(playerPos, bulletManager, 3);
    if (cycle == 295) summonRequests_ += 2;

    CheckCollision(obstacles);
    const Vector3 currentPos = GetPosition();
    SetVelocity({
        (currentPos.x - prevPos.x) * 60.0f,
        (currentPos.y - prevPos.y) * 60.0f,
        (currentPos.z - prevPos.z) * 60.0f,
    });
    UpdateRearThrusters();
    UpdateModel();
}

Vector3 Boss::DirectionTo(const Vector3 &target) const {
    const Vector3 pos = GetPosition();
    Vector3 d{ target.x - pos.x, target.y - pos.y, target.z - pos.z };
    const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    return length > 0.0001f ? ScaleVector(d, 1.0f / length) : Vector3{ 0.0f, 0.0f, -1.0f };
}

Vector3 Boss::LocalPointToWorld(const Vector3 &localPoint) const {
    const Vector3 scaled = {
        localPoint.x * kBossScale.x,
        localPoint.y * kBossScale.y,
        localPoint.z * kBossScale.z,
    };
    return AddVector(GetPosition(), MyMath::RotateVector(scaled, MakeEulerQuaternion(GetRotation())));
}

void Boss::TakeDamageAt(int damage, const Vector3 &hitPosition, const Vector3 &hitDirection, float impactRadius) {
    if (damage <= 0 || IsDead()) return;

    size_t shieldFace = 0;
    if (TryGetSideShieldFaceAtWorldPosition(hitPosition, impactRadius, shieldFace) &&
        IsSideShieldProtectingFace(shieldFace)) {
        // 展開開始から解除予告の間は側面からの着弾を受け止める。
        return;
    }

    for (size_t face = 0; face < kSideShieldGeneratorCount; ++face) {
        if (!IsSideShieldGeneratorVulnerable(face)) continue;
        const Vector3 offset = SubtractVector(hitPosition, GetLockOnPartPosition(kTurretCount + face));
        const float radius = GetLockOnPartRadius(kTurretCount + face);
        if (DotVector(offset, offset) <= radius * radius) {
            ApplySideShieldGeneratorDamage(face, damage);
            return;
        }
    }

    for (size_t index = 0; index < GetTurretCount(); ++index) {
        if (!IsTurretAlive(index)) continue;
        const Vector3 center = LocalPointToWorld(GetTurretPivot(index));
        const Vector3 offset = SubtractVector(hitPosition, center);
        const float hitRadius = GetTurretHitRadius(index) * (std::max)(kBossScale.x, kBossScale.z);
        if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z > hitRadius * hitRadius) continue;

        ApplyTurretDamage(index, damage);
        // 部位に当たった弾はボス本体へは通さない。
        return;
    }

    Enemy::TakeDamageAt(damage, hitPosition, hitDirection, impactRadius);
}

void Boss::TakeDamageAtTargetPoint(int damage, const Vector3& targetPoint,
    const Vector3& hitPosition, const Vector3& hitDirection, float impactRadius) {
    if (damage <= 0 || IsDead()) return;

    size_t shieldFace = 0;
    if (TryGetSideShieldFaceAtWorldPosition(hitPosition, impactRadius, shieldFace) &&
        IsSideShieldProtectingFace(shieldFace)) {
        return;
    }

    // 部位ロック中は、弾が船体 OBB に先に触れてもロックした砲台・発生装置へ確実にダメージを渡す。
    for (size_t index = 0; index < GetLockOnPartCount(); ++index) {
        if (!IsLockOnPartAvailable(index)) continue;
        const Vector3 offset = SubtractVector(targetPoint, GetLockOnPartPosition(index));
        const float lockRadius = GetLockOnPartRadius(index);
        if (DotVector(offset, offset) <= lockRadius * lockRadius) {
            if (index < kTurretCount) {
                ApplyTurretDamage(index, damage);
            } else {
                ApplySideShieldGeneratorDamage(index - kTurretCount, damage);
            }
            return;
        }
    }

    // ロック対象が既に破壊済みなどで特定できなければ、通常の着弾判定に戻す。
    TakeDamageAt(damage, hitPosition, hitDirection, impactRadius);
}

bool Boss::ApplyTurretDamage(size_t turretIndex, int damage) {
    if (turretIndex >= GetTurretCount() || !IsTurretAlive(turretIndex) || damage <= 0) return false;
    turretHP_[turretIndex] = (std::max)(0, turretHP_[turretIndex] - damage);
    turretDestroyed_[turretIndex] = turretHP_[turretIndex] == 0;
    return true;
}

bool Boss::IsHitBy(const Sphere& attackSphere) const {
    size_t shieldFace = 0;
    if (TryGetSideShieldFaceAtWorldPosition(attackSphere.center, attackSphere.radius, shieldFace) &&
        IsSideShieldProtectingFace(shieldFace)) {
        return true;
    }

    // 側面発生装置は非展開中だけ、船体外の独立した当たり判定として扱う。
    for (size_t face = 0; face < kSideShieldGeneratorCount; ++face) {
        if (!IsSideShieldGeneratorVulnerable(face)) continue;
        const Vector3 offset = SubtractVector(attackSphere.center, GetLockOnPartPosition(kTurretCount + face));
        const float hitRadius = GetLockOnPartRadius(kTurretCount + face) + attackSphere.radius;
        if (DotVector(offset, offset) <= hitRadius * hitRadius) return true;
    }

    // 砲台は船体から張り出しているため、本体 OBB の外で命中しても部位ダメージへ到達できる。
    for (size_t index = 0; index < GetTurretCount(); ++index) {
        if (!IsTurretAlive(index)) continue;
        const Vector3 offset = SubtractVector(attackSphere.center, GetTurretLockOnPosition(index));
        const float hitRadius = GetTurretLockOnRadius(index) + attackSphere.radius;
        if (DotVector(offset, offset) <= hitRadius * hitRadius) {
            return true;
        }
    }

    // 砲台以外は実際に描画している船体と同じスケールの OBB を使う。
    return MyMath::IsCollision(attackSphere, GetOBB());
}

OBB Boss::GetOBB() const {
    OBB obb{};
    // ボス船体の見た目の中心（BoundsCenter）にOBBの中心を完全に一致させる
    obb.center = GetBodyLockOnPosition();
    obb.size = GetWorldHalfExtents();

    const Vector3 rot = GetRotation();
    const Matrix4x4 rotationMatrix = MyMath::Multiply(
        MyMath::Multiply(MyMath::MakeRoteXMatrix(rot.x), MyMath::MakeRotateYMatrix(rot.y)),
        MyMath::MakeRotateZMatrix(rot.z));
    obb.orientations[0] = MyMath::Normalize(Vector3{ rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2] });
    obb.orientations[1] = MyMath::Normalize(Vector3{ rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2] });
    obb.orientations[2] = MyMath::Normalize(Vector3{ rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2] });

    return obb;
}

bool Boss::IsTurretAlive(size_t turretIndex) const {
    return turretIndex < GetTurretCount() && !turretDestroyed_[turretIndex];
}

bool Boss::IsSideShieldGeneratorDestroyed(size_t face) const {
    return face >= kSideShieldGeneratorCount || sideShieldGeneratorDestroyed_[face];
}

bool Boss::IsLockOnPartAvailable(size_t partIndex) const {
    if (partIndex < kTurretCount) return IsTurretAlive(partIndex);
    return IsSideShieldGeneratorVulnerable(partIndex - kTurretCount);
}

Vector3 Boss::GetLockOnPartPosition(size_t partIndex) const {
    if (partIndex < kTurretCount) return GetTurretLockOnPosition(partIndex);
    const size_t face = partIndex - kTurretCount;
    if (face >= kSideShieldGeneratorCount) return GetPosition();
    return LocalPointToWorld(kSideShieldPlaneCenters[face]);
}

float Boss::GetLockOnPartRadius(size_t partIndex) const {
    if (partIndex < kTurretCount) return GetTurretLockOnRadius(partIndex);
    const size_t face = partIndex - kTurretCount;
    return face < kSideShieldGeneratorCount ? kSideShieldGeneratorLockOnRadius : GetCollisionRadius();
}

int Boss::GetLockOnPartHP(size_t partIndex) const {
    if (partIndex < kTurretCount) return GetTurretHP(partIndex);
    const size_t face = partIndex - kTurretCount;
    return face < kSideShieldGeneratorCount ? sideShieldGeneratorHP_[face] : 0;
}

int Boss::GetLockOnPartMaxHP(size_t partIndex) const {
    return partIndex < kTurretCount ? kTurretMaxHP :
        (partIndex < GetLockOnPartCount() ? kSideShieldGeneratorMaxHP : 0);
}

Vector3 Boss::GetBodyLockOnPosition() const {
    // ボス OBJ は原点が船体下側にあるため、そのまま position_ を使うと
    // ロックオンとカメラの注視点がモデルより下へずれる。
    const Object3d* obj = GetObject3d();
    if (!obj || !obj->GetModel()) return GetPosition();
    return LocalPointToWorld(obj->GetModel()->GetBoundsCenter());
}

Vector3 Boss::GetTurretLockOnPosition(size_t turretIndex) const {
    if (!IsTurretAlive(turretIndex)) return GetPosition();
    return LocalPointToWorld(GetTurretPivot(turretIndex));
}

float Boss::GetTurretLockOnRadius(size_t turretIndex) const {
    if (!IsTurretAlive(turretIndex)) return GetCollisionRadius();
    return GetTurretHitRadius(turretIndex) * (std::max)(kBossScale.x, kBossScale.z);
}

int Boss::GetTurretHP(size_t turretIndex) const {
    return turretIndex < turretHP_.size() ? turretHP_[turretIndex] : 0;
}

int Boss::GetTurretMaxHP() const {
    return kTurretMaxHP;
}

void Boss::SetTuning(const Tuning& tuning) {
    const int previousMaxHP = tuning_.maxHP;
    tuning_ = tuning;
    tuning_.maxHP = std::clamp(tuning_.maxHP, 1, 1000000);
    // HP設定を変更した時だけ満タンにする。外観調整で戦闘中のHPを回復させない。
    if (previousMaxHP != tuning_.maxHP && !IsDead()) {
        SetHP(tuning_.maxHP);
    }
    // 縦レール移動は廃止。旧JSONでtrueでも、砲台は必ずピッチ回転で照準する。
    tuning_.forwardTurretsVerticalSlide = false;
    tuning_.mainTurretYawFollow = std::clamp(tuning_.mainTurretYawFollow, 0.0f, 1.0f);
    tuning_.mainTurretPitchFollow = std::clamp(tuning_.mainTurretPitchFollow, 0.0f, 1.0f);
    tuning_.mainTurretMaxPitch = std::clamp(tuning_.mainTurretMaxPitch, 0.0f, 1.45f);
    tuning_.forwardTurretSlideAmount = std::clamp(tuning_.forwardTurretSlideAmount, 0.0f, 1.20f);
    tuning_.forwardTurretSlideSpeed = std::clamp(tuning_.forwardTurretSlideSpeed, 0.0f, 0.50f);
    tuning_.forwardTurretAimSlide = std::clamp(tuning_.forwardTurretAimSlide, 0.0f, 2.0f);
    tuning_.rearThrusterCount = std::clamp(tuning_.rearThrusterCount, 0, static_cast<int>(kMaxRearThrusters));
    tuning_.rearThrusterSpacing = std::clamp(tuning_.rearThrusterSpacing, 0.05f, 1.20f);
    tuning_.rearThrusterRadius = std::clamp(tuning_.rearThrusterRadius, 0.02f, 1.50f);
    tuning_.rearThrusterLength = std::clamp(tuning_.rearThrusterLength, 0.05f, 8.0f);
    tuning_.rearThrusterBrightness = std::clamp(tuning_.rearThrusterBrightness, 0.0f, 3.0f);
    tuning_.rearThrusterFlareRadius = std::clamp(tuning_.rearThrusterFlareRadius, 0.02f, 2.0f);
    tuning_.sideShieldOpacity = std::clamp(tuning_.sideShieldOpacity, 0.05f, 1.0f);
    UpdateRearThrusters();
}

Vector3 Boss::GetRearThrusterWorldPosition(size_t thrusterIndex) const {
    if (thrusterIndex >= GetRearThrusterCount()) return GetPosition();
    return rearThrusterOrigins_[thrusterIndex];
}

void Boss::UpdateCinematicDebris() {
    StepCinematicDebris(1.0f / 60.0f);
}

void Boss::InitializeRearThrusters() {
    ModelManager* modelManager = ModelManager::GetInstance();
    for (size_t index = 0; index < rearThrusterTrails_.size(); ++index) {
        const std::string flameModel = "BossRearThrusterFlame_" + std::to_string(index);
        const std::string flareModel = "BossRearThrusterFlare_" + std::to_string(index);
        modelManager->CreateTrailModel(flameModel);
        modelManager->CreateTrailModel(flareModel);

        rearThrusterTrails_[index] = std::make_unique<Trail>();
        rearThrusterTrails_[index]->Initialize(2);
        rearThrusterObjects_[index] = std::make_unique<Object3d>();
        rearThrusterObjects_[index]->Initialize(Object3dCommon::GetInstance());
        rearThrusterObjects_[index]->SetModel(flameModel);
        rearThrusterFlareObjects_[index] = std::make_unique<Object3d>();
        rearThrusterFlareObjects_[index]->Initialize(Object3dCommon::GetInstance());
        rearThrusterFlareObjects_[index]->SetModel(flareModel);
    }
}

void Boss::UpdateRearThrusters() {
    for (size_t index = 0; index < rearThrusterOrigins_.size(); ++index) {
        // 全12基をそれぞれ対応する四角い穴へ置く。
        const size_t panelIndex = index;
        const Vector3& individualOffset = tuning_.rearThrusterIndividualOffsets[index];
        const Vector3 localOrigin = {
            kRearThrusterPanelCenters[panelIndex].x + tuning_.rearThrusterOffset.x + individualOffset.x,
            kRearThrusterPanelCenters[panelIndex].y + tuning_.rearThrusterOffset.y + individualOffset.y,
            kRearThrusterPanelCenters[panelIndex].z + tuning_.rearThrusterOffset.z + individualOffset.z,
        };
        rearThrusterOrigins_[index] = LocalPointToWorld(localOrigin);
        if (rearThrusterTrails_[index]) {
            // 炎を艦体へ完全に固定し、移動時に不要な航跡を残さない。
            rearThrusterTrails_[index]->SetOrigin(rearThrusterOrigins_[index]);
        }
    }
}

void Boss::DrawRearThrusters() {
    Object3dCommon* object3dCommon = Object3dCommon::GetInstance();
    Camera* camera = object3dCommon->GetDefaultCamera();
    if (!camera) return;

    const Vector3 exhaustDirection = RotatePitchYaw(kRearThrusterDirection, 0.0f, GetRotation().y);
    object3dCommon->SetThrusterDrawSettings();
    for (size_t index = 0; index < static_cast<size_t>(tuning_.rearThrusterCount); ++index) {
        Trail* trail = rearThrusterTrails_[index].get();
        Object3d* flame = rearThrusterObjects_[index].get();
        Object3d* flare = rearThrusterFlareObjects_[index].get();
        if (!trail || !flame || !flare || !flame->GetModel() || !flare->GetModel()) continue;

        const float phase = static_cast<float>(actionTimer_) * 0.14f + static_cast<float>(index) * 1.7f;
        const std::vector<VertexData> flameVertices = trail->GenerateThrusterVertices(
            camera, tuning_.rearThrusterRadius, exhaustDirection, tuning_.rearThrusterLength, phase,
            tuning_.rearThrusterBrightness, 0.54f, 1.20f);
        if (!flameVertices.empty()) {
            flame->SetTranslate({ 0.0f, 0.0f, 0.0f });
            flame->SetScale({ 1.0f, 1.0f, 1.0f });
            flame->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
            flame->GetModel()->SetColor({ 0.72f, 0.92f, 1.0f, 1.0f });
            flame->GetModel()->UpdateTrailVertices(flameVertices, false);
            flame->Update(camera);
            flame->Draw();
        }

        const std::vector<VertexData> flareVertices = trail->GenerateThrusterFlareVertices(
            camera, rearThrusterOrigins_[index], tuning_.rearThrusterFlareRadius,
            tuning_.rearThrusterBrightness * 0.91f);
        if (!flareVertices.empty()) {
            object3dCommon->SetSpeedTrailDrawSettings();
            flare->SetTranslate({ 0.0f, 0.0f, 0.0f });
            flare->SetScale({ 1.0f, 1.0f, 1.0f });
            flare->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
            flare->GetModel()->SetColor({ 0.72f, 0.92f, 1.0f, 1.0f });
            flare->GetModel()->UpdateTrailVertices(flareVertices, true);
            flare->Update(camera);
            flare->Draw();
            object3dCommon->SetThrusterDrawSettings();
        }
    }
    object3dCommon->SetCommonDrawSettings();
}

int Boss::ConsumeSummonRequests() {
    const int result = summonRequests_;
    summonRequests_ = 0;
    return result;
}

void Boss::UpdateModel() {
    Enemy::UpdateModel();
    if (cinematicDebrisActive_) {
        if (Object3d* obj = GetObject3d()) {
            // Enemy::UpdateModel() は通常船体のスケールを復元するため、撃破中だけ隠し直す。
            obj->SetScale({ 0.0f, 0.0f, 0.0f });
            obj->Update();
        }
    }
    UpdateRearThrusters();
    UpdateTurretModels();
    UpdateSideShields();
}

void Boss::Draw() {
    if (cinematicDebrisActive_) {
        for (Object3d* structuralPart : cinematicDebrisParts_) {
            if (structuralPart) structuralPart->Draw();
        }
        for (const auto& smallPart : cinematicSmallDebris_) {
            if (smallPart) smallPart->Draw();
        }
        return;
    }
    Enemy::Draw();
    for (size_t index = 0; index < mainTurrets_.size(); ++index) {
        if (IsTurretAlive(kMainTurretFirst + index) && mainTurrets_[index]) mainTurrets_[index]->Draw();
    }
    for (size_t index = 0; index < miniTurretBases_.size(); ++index) {
        if (IsTurretAlive(kMiniTurretFirst + index) && miniTurretBases_[index]) miniTurretBases_[index]->Draw();
    }
    for (size_t index = 0; index < miniTurretBarrels_.size(); ++index) {
        if (IsTurretAlive(kMiniTurretFirst + index) && miniTurretBarrels_[index]) miniTurretBarrels_[index]->Draw();
    }
    if (IsTurretAlive(kMachineGunTurretIndex) && machineGunTurret_) machineGunTurret_->Draw();
    for (size_t index = 0; index < forwardTurrets_.size(); ++index) {
        if (IsTurretAlive(kForwardTurretFirst + index) && forwardTurrets_[index]) {
            forwardTurrets_[index]->Draw();
        }
    }
    for (size_t face = 0; face < kSideShieldGeneratorCount; ++face) {
        if (!sideShieldGeneratorDestroyed_[face] && sideShieldGenerators_[face]) {
            sideShieldGenerators_[face]->Draw();
        }
    }
    if (!cinematicWreckPresentation_) {
        DrawRearThrusters();
    }
}

void Boss::DrawTransparent() {
    // 地形などすべての不透明物が深度を書き終えた後で描く。
    // これによりシールドは地形の奥では隠れ、手前では上書きされずに表示される。
    if (cinematicDebrisActive_ || cinematicWreckPresentation_) {
        return;
    }
    DrawSideShields();
}
