#include "Boss.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/EnemyBulletManager.h"
#include <algorithm>
#include <array>
#include <cmath>
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

// 砲身が砲台の旋回支点側を向いているときは、弾の向きを勝手に曲げず発射そのものを止める。
// 艦体中心では判定しないので、艦首・艦尾に正しく配置された砲身は発射できる。
bool IsMuzzleFacingAwayFromPivot(const Vector3& pivotPosition, const Vector3& muzzlePosition,
    const Vector3& barrelDirection) {
    const Vector3 outward = NormalizeVector(SubtractVector(muzzlePosition, pivotPosition));
    return DotVector(barrelDirection, outward) > 0.10f;
}

// Boss1 の全長約8.5mを、プレイヤーを大きく上回る戦艦サイズまで拡大する。
constexpr Vector3 kBossScale = { 1.85f, 1.55f, 1.85f };
constexpr float kBossSidewaysYaw = 1.57079632679f; // 90 degrees

// OBJ内の実オブジェクト順を基準にする。船体・甲板・側面装甲は残し、砲台だけを
// 別Object3dとして回す。これによりBlenderの完成形を保ったまま旋回できる。
constexpr char kBossBodyModel[] = "Boss1/Boss1.obj#exclude=0,2-13,19-20,22,29-30";
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

struct WeaponMount {
    Vector3 pivot;
    std::array<Vector3, 2> muzzles;
    std::array<Vector3, 2> barrelAxes = { Vector3{ 0.0f, 0.0f, 1.0f }, Vector3{ 0.0f, 0.0f, 1.0f } };
    float modelPitchOffset = 0.0f;
};

// OBJ ローダーがX軸を反転するため、ここではゲーム座標へ変換済みの位置を使う。
constexpr std::array<WeaponMount, 4> kMainWeaponMounts = {
    WeaponMount{ Vector3{ -1.73f, 3.85f, -2.62f }, std::array<Vector3, 2>{ Vector3{ -1.41f, 4.30f, -0.48f }, Vector3{ -2.10f, 4.30f, -0.48f } } },
    WeaponMount{ Vector3{  1.83f, 3.85f, -2.62f }, std::array<Vector3, 2>{ Vector3{  1.47f, 4.30f, -0.48f }, Vector3{  2.15f, 4.30f, -0.48f } } },
    WeaponMount{ Vector3{  0.04f, 2.38f,  2.26f }, std::array<Vector3, 2>{ Vector3{ -0.38f, 2.07f,  4.60f }, Vector3{  0.31f, 2.07f,  4.60f } } },
    // 選択されている上部ミニ連装砲。左右の砲身は外上方へ開く形なので、
    // 一律の前方(+Z)ではなくOBJの実際の砲身軸をそれぞれ使う。
    WeaponMount{ Vector3{  0.02f, 4.45f, -0.76f },
        std::array<Vector3, 2>{ Vector3{ -0.89f, 4.89f, -0.78f }, Vector3{  0.93f, 4.89f, -0.78f } },
        std::array<Vector3, 2>{ Vector3{ -0.634f, 0.773f, 0.0f }, Vector3{ 0.634f, 0.773f, 0.0f } } },
};
// 上部ミニ連装砲の各砲身のヒンジ。砲身だけをここで上下へ回す。
constexpr std::array<Vector3, 2> kMiniTurretBarrelPivots = {
    Vector3{ -0.72f, 4.34f, -0.78f }, Vector3{ 0.70f, 4.34f, -0.78f },
};
constexpr std::array<Vector3, 2> kMiniTurretBasePivots = {
    Vector3{ -0.59f, 4.30f, -0.76f }, Vector3{ 0.58f, 4.30f, -0.76f },
};
constexpr std::array<float, 3> kMainTurretBaseYaws = {
    0.0f, 0.0f, 0.0f,
};
// 左右の大型砲は外側を向く90度を基準にし、艦体側を通らない半円だけを使う。
// エンジンのY回転はBlenderのZ回転に対応する。
constexpr std::array<float, 3> kMainTurretYawMinimums = {
    -1.57079632679f, -1.57079632679f, -1.57079632679f,
};
constexpr std::array<float, 3> kMainTurretYawMaximums = {
    1.57079632679f, 1.57079632679f, 1.57079632679f,
};
constexpr std::array<float, 2> kMiniTurretBaseYaws = {
    0.0f, 0.0f,
};
constexpr WeaponMount kMachineGunMount = WeaponMount{
    Vector3{ 0.01f, 4.70f, 0.90f }, std::array<Vector3, 2>{ Vector3{ 0.29f, 5.22f, 1.78f }, Vector3{ -0.28f, 5.22f, 1.78f } }
};

struct ForwardWeaponMount {
    Vector3 pivot;
    Vector3 muzzle;
    float hitRadius;
};

constexpr std::array<ForwardWeaponMount, 3> kForwardWeaponMounts = {
    ForwardWeaponMount{ Vector3{ 0.02f, 4.42f, 3.23f }, Vector3{ 0.02f, 4.55f, 4.20f }, 1.00f },
    ForwardWeaponMount{ Vector3{ -1.31f, 3.28f, 3.57f }, Vector3{ -1.31f, 3.28f, 4.92f }, 0.82f },
    ForwardWeaponMount{ Vector3{ 1.33f, 3.28f, 3.57f }, Vector3{ 1.33f, 3.28f, 4.92f }, 0.82f },
};
constexpr int kTurretMaxHP = 12;

// ロックオン、被弾判定、描画・発射停止で共有する砲台番号。
constexpr size_t kForwardTurretFirst = 0;
constexpr size_t kMainTurretFirst = kForwardTurretFirst + kForwardWeaponMounts.size();
constexpr size_t kMiniTurretFirst = kMainTurretFirst + kMainWeaponMounts.size() - 1;
constexpr size_t kMachineGunTurretIndex = kMiniTurretFirst + kMiniTurretBarrelPivots.size();

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

    // Boss1 の船体と可動砲塔を別モデルとして読み込む。
    ModelManager *modelManager = ModelManager::GetInstance();
    if (!modelManager->FindModel(kBossBodyModel)) {
        modelManager->LoadModel(kBossBodyModel);
    }
    object_->SetModel(kBossBodyModel);
    if (object_->GetModel()) {
        object_->GetModel()->SetColor({ 0.20f, 0.33f, 0.39f, 1.0f });
        object_->GetModel()->SetShininess(96.0f);
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
    InitializeRearThrusters();

    SetIsBoss(true);
    // 本体と砲台が同じ戦艦スケールを使用するよう、描画だけでなく当たり判定の
    // 基準になる Enemy::scale_ も同期する。
    SetScale(kBossScale);
    hp_ = kMaxHP;
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
    UpdateRearThrusters();
    UpdateModel();
}

void Boss::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    if (isDead_) return;

    ++actionTimer_;
    position_.y += std::sin(static_cast<float>(actionTimer_) * 0.012f) * 0.008f;
    UpdateTurretAim(playerPos);
    // 以前の4秒周期では同時弾数が多過ぎたため、5秒周期へ広げて各砲の間隔も空ける。
    const int cycle = actionTimer_ % 300;
    if (cycle >= 25 && cycle < 85 && cycle % 10 == 0) FireMachineGun(playerPos, bulletManager);
    if (cycle == 110) FireForwardTurret(playerPos, bulletManager, 0);
    if (cycle == 125) FireForwardTurret(playerPos, bulletManager, 1);
    if (cycle == 140) FireForwardTurret(playerPos, bulletManager, 2);
    if (cycle == 170 && mainTurretEngaged_[0]) FireMainBeam(playerPos, bulletManager, 0);
    if (cycle == 200 && mainTurretEngaged_[1]) FireMainBeam(playerPos, bulletManager, 1);
    if (cycle == 230 && mainTurretEngaged_[2]) FireMainBeam(playerPos, bulletManager, 2);
    // 上部ミニ連装砲は短い4連射だけにして、回避可能な弾幕密度にする。
    if (cycle >= 250 && cycle < 295 && (cycle - 250) % 12 == 0) FireMainBeam(playerPos, bulletManager, 3);
    if (cycle == 295) summonRequests_ += 2;

    CheckCollision(obstacles);
    UpdateRearThrusters();
    UpdateModel();
}

Vector3 Boss::DirectionTo(const Vector3 &target) const {
    Vector3 d{ target.x - position_.x, target.y - position_.y, target.z - position_.z };
    const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    return length > 0.0001f ? ScaleVector(d, 1.0f / length) : Vector3{ 0.0f, 0.0f, -1.0f };
}

Vector3 Boss::LocalPointToWorld(const Vector3 &localPoint) const {
    return AddVector(position_, RotatePitchYaw({
        localPoint.x * kBossScale.x, localPoint.y * kBossScale.y, localPoint.z * kBossScale.z
    }, 0.0f, rotation_.y));
}

void Boss::FireMachineGun(const Vector3 &playerPos, EnemyBulletManager *bulletManager) {
    if (!bulletManager || !IsTurretAlive(kMachineGunTurretIndex)) return;
    (void)playerPos;
    const float pitch = kMachineGunMount.modelPitchOffset;
    const Vector3 pivotPosition = LocalPointToWorld(kMachineGunMount.pivot);
    for (const Vector3& localMuzzle : kMachineGunMount.muzzles) {
        const Vector3 rotatedMuzzle = AddVector(kMachineGunMount.pivot,
            RotatePitchYaw(SubtractVector(localMuzzle, kMachineGunMount.pivot), pitch, turretYaw_));
        const Vector3 muzzlePosition = AddVector(position_, RotatePitchYaw({
            rotatedMuzzle.x * kBossScale.x, rotatedMuzzle.y * kBossScale.y, rotatedMuzzle.z * kBossScale.z
        }, 0.0f, rotation_.y));
        const Vector3 localDirection = RotatePitchYaw(kMachineGunMount.barrelAxes[0], pitch, turretYaw_);
        const Vector3 direction = RotatePitchYaw(localDirection, 0.0f, rotation_.y);
        if (!IsMuzzleFacingAwayFromPivot(pivotPosition, muzzlePosition, direction)) continue;
        bulletManager->Shoot(muzzlePosition, ScaleVector(direction, 0.70f));
    }
}

void Boss::FireMainBeam(const Vector3 &playerPos, EnemyBulletManager *bulletManager, size_t turretIndex) {
    if (!bulletManager || turretIndex >= kMainWeaponMounts.size()) return;
    if (turretIndex < mainTurrets_.size() && !IsTurretAlive(kMainTurretFirst + turretIndex)) return;
    (void)playerPos;
    const WeaponMount& mount = kMainWeaponMounts[turretIndex];
    // 主砲はヨーだけ。上部ミニ連装砲は台座ヨー + 砲身ピッチの二段旋回。
    for (size_t muzzleIndex = 0; muzzleIndex < mount.muzzles.size(); ++muzzleIndex) {
        if (turretIndex == mainTurrets_.size() && !IsTurretAlive(kMiniTurretFirst + muzzleIndex)) continue;
        const Vector3& localMuzzle = mount.muzzles[muzzleIndex];
        Vector3 rotatedMuzzle{};
        Vector3 pivotPosition{};
        float pitch = mount.modelPitchOffset;
        float yaw = turretIndex < mainTurretYaws_.size()
            ? kMainTurretBaseYaws[turretIndex] + mainTurretYaws_[turretIndex]
            : turretYaw_;
        if (turretIndex == 3) {
            pitch = miniTurretPitches_[muzzleIndex];
            yaw = kMiniTurretBaseYaws[muzzleIndex] + miniTurretYaws_[muzzleIndex];
            const Vector3& barrelPivot = kMiniTurretBarrelPivots[muzzleIndex];
            const Vector3& basePivot = kMiniTurretBasePivots[muzzleIndex];
            // 台座は横旋回のみ、砲身は自身のヒンジを中心に上下旋回する。
            const Vector3 rotatedBarrelPivot = AddVector(basePivot,
                RotatePitchYaw(SubtractVector(barrelPivot, basePivot), 0.0f, yaw));
            rotatedMuzzle = AddVector(rotatedBarrelPivot,
                RotatePitchYaw(SubtractVector(localMuzzle, barrelPivot), pitch, yaw));
            pivotPosition = AddVector(position_, RotatePitchYaw({
                rotatedBarrelPivot.x * kBossScale.x, rotatedBarrelPivot.y * kBossScale.y,
                rotatedBarrelPivot.z * kBossScale.z
            }, 0.0f, rotation_.y));
        } else {
            rotatedMuzzle = AddVector(mount.pivot,
                RotatePitchYaw(SubtractVector(localMuzzle, mount.pivot), pitch, yaw));
            pivotPosition = LocalPointToWorld(mount.pivot);
        }
        const Vector3 muzzlePosition = AddVector(position_, RotatePitchYaw({
            rotatedMuzzle.x * kBossScale.x, rotatedMuzzle.y * kBossScale.y, rotatedMuzzle.z * kBossScale.z
        }, 0.0f, rotation_.y));
        const Vector3 localDirection = RotatePitchYaw(mount.barrelAxes[muzzleIndex], pitch, yaw);
        const Vector3 direction = RotatePitchYaw(localDirection, 0.0f, rotation_.y);
        if (!IsMuzzleFacingAwayFromPivot(pivotPosition, muzzlePosition, direction)) continue;
        if (turretIndex == 3) {
            bulletManager->Shoot(muzzlePosition, ScaleVector(direction, 0.88f));
        } else {
            // 巨大なビーム用の板ポリゴンはスラスターと見分けが付きにくく、
            // 停止中に艦体から離れたように見えるため、主砲も通常弾として描画する。
            bulletManager->Shoot(muzzlePosition, ScaleVector(direction, 0.92f));
        }
    }
}

void Boss::FireForwardTurret(const Vector3 &playerPos, EnemyBulletManager *bulletManager, size_t turretIndex) {
    if (!bulletManager || turretIndex >= kForwardWeaponMounts.size() ||
        !IsTurretAlive(kForwardTurretFirst + turretIndex)) return;
    (void)playerPos;

    const ForwardWeaponMount& mount = kForwardWeaponMounts[turretIndex];
    // 画像の左右砲台（1, 2）は艦体に対する横向きを固定し、上下だけを回転する。
    // 中央砲台（0）は横回転だけで追尾する。
    const float yaw = turretIndex > 0 ? 0.0f : turretYaw_;
    const float pitch = turretIndex > 0 ? turretPitch_ : 0.0f;
    const Vector3 rotatedMuzzle = AddVector(mount.pivot,
        RotatePitchYaw(SubtractVector(mount.muzzle, mount.pivot), pitch, yaw));
    const Vector3 muzzlePosition = LocalPointToWorld(rotatedMuzzle);
    const Vector3 pivotPosition = LocalPointToWorld(mount.pivot);
    const Vector3 localDirection = RotatePitchYaw({ 0.0f, 0.0f, 1.0f }, pitch, yaw);
    const Vector3 direction = RotatePitchYaw(localDirection, 0.0f, rotation_.y);
    if (!IsMuzzleFacingAwayFromPivot(pivotPosition, muzzlePosition, direction)) return;
    bulletManager->Shoot(muzzlePosition, ScaleVector(direction, 0.70f));
}

void Boss::TakeDamageAt(int damage, const Vector3 &hitPosition, const Vector3 &hitDirection) {
    if (damage <= 0 || isDead_) return;

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

    Enemy::TakeDamageAt(damage, hitPosition, hitDirection);
}

void Boss::TakeDamageAtTargetPoint(int damage, const Vector3& targetPoint,
    const Vector3& hitPosition, const Vector3& hitDirection) {
    if (damage <= 0 || isDead_) return;

    // 部位ロック中は、弾が船体 OBB に先に触れてもロックした砲台へ確実にダメージを渡す。
    // targetPoint はロック開始時の砲台支点なので、砲台の描画・当たり判定と同じ座標系で照合する。
    for (size_t index = 0; index < GetTurretCount(); ++index) {
        if (!IsTurretAlive(index)) continue;
        const Vector3 offset = SubtractVector(targetPoint, GetTurretLockOnPosition(index));
        const float lockRadius = GetTurretLockOnRadius(index);
        if (DotVector(offset, offset) <= lockRadius * lockRadius) {
            ApplyTurretDamage(index, damage);
            return;
        }
    }

    // ロック対象が既に破壊済みなどで特定できなければ、通常の着弾判定に戻す。
    TakeDamageAt(damage, hitPosition, hitDirection);
}

bool Boss::ApplyTurretDamage(size_t turretIndex, int damage) {
    if (turretIndex >= GetTurretCount() || !IsTurretAlive(turretIndex) || damage <= 0) return false;
    turretHP_[turretIndex] = (std::max)(0, turretHP_[turretIndex] - damage);
    turretDestroyed_[turretIndex] = turretHP_[turretIndex] == 0;
    return true;
}

bool Boss::IsHitBy(const Sphere& attackSphere) const {
    // 砲台は船体から張り出しているため、本体 OBB の外で命中しても部位ダメージへ
    // 到達できるよう、各砲台の球判定を先に確認する。
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

    const Matrix4x4 rotationMatrix = MyMath::Multiply(
        MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::MakeRotateYMatrix(rotation_.y)),
        MyMath::MakeRotateZMatrix(rotation_.z));
    obb.orientations[0] = MyMath::Normalize(Vector3{ rotationMatrix.m[0][0], rotationMatrix.m[0][1], rotationMatrix.m[0][2] });
    obb.orientations[1] = MyMath::Normalize(Vector3{ rotationMatrix.m[1][0], rotationMatrix.m[1][1], rotationMatrix.m[1][2] });
    obb.orientations[2] = MyMath::Normalize(Vector3{ rotationMatrix.m[2][0], rotationMatrix.m[2][1], rotationMatrix.m[2][2] });

    return obb;
}

bool Boss::IsTurretAlive(size_t turretIndex) const {
    return turretIndex < GetTurretCount() && !turretDestroyed_[turretIndex];
}

Vector3 Boss::GetBodyLockOnPosition() const {
    // ボス OBJ は原点が船体下側にあるため、そのまま position_ を使うと
    // ロックオンとカメラの注視点がモデルより下へずれる。
    if (!object_ || !object_->GetModel()) return position_;
    return LocalPointToWorld(object_->GetModel()->GetBoundsCenter());
}

Vector3 Boss::GetTurretLockOnPosition(size_t turretIndex) const {
    if (!IsTurretAlive(turretIndex)) return position_;
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
    tuning_ = tuning;
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
    UpdateRearThrusters();
}

Vector3 Boss::GetRearThrusterWorldPosition(size_t thrusterIndex) const {
    if (thrusterIndex >= GetRearThrusterCount()) return position_;
    return rearThrusterOrigins_[thrusterIndex];
}

Vector3 Boss::GetTurretPivot(size_t turretIndex) const {
    if (turretIndex < kForwardWeaponMounts.size()) return kForwardWeaponMounts[turretIndex].pivot;
    if (turretIndex < kMiniTurretFirst) return kMainWeaponMounts[turretIndex - kMainTurretFirst].pivot;
    if (turretIndex < kMachineGunTurretIndex) return kMiniTurretBasePivots[turretIndex - kMiniTurretFirst];
    if (turretIndex == kMachineGunTurretIndex) return kMachineGunMount.pivot;
    return { 0.0f, 0.0f, 0.0f };
}

float Boss::GetTurretHitRadius(size_t turretIndex) const {
    if (turretIndex < kForwardWeaponMounts.size()) return kForwardWeaponMounts[turretIndex].hitRadius;
    if (turretIndex < kMiniTurretFirst) return 0.95f;
    if (turretIndex < kMachineGunTurretIndex) return 0.58f;
    if (turretIndex == kMachineGunTurretIndex) return 0.72f;
    return 0.0f;
}

void Boss::UpdateTurretAim(const Vector3& playerPos) {
    const Vector3 direction = DirectionTo(playerPos);
    const float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (horizontalLength <= 0.0001f) return;

    const float requestedYaw = tuning_.mainTurretYawTracking
        ? WrapAngle(std::atan2(direction.x, direction.z) - rotation_.y) : 0.0f;
    const float requestedPitch = tuning_.mainTurretPitchTracking
        ? std::clamp(-std::atan2(direction.y, horizontalLength), -tuning_.mainTurretMaxPitch, tuning_.mainTurretMaxPitch) : 0.0f;
    constexpr float kQuarterPi = 0.78539816339f;
    // ミニ連装砲は初期姿勢が上向き。+90度を基準にすることで砲身を前方へ倒し、
    // プレイヤーが下にいる場合もさらに下向きまで追尾できる。
    constexpr float kIgelForwardPitch = 1.57079632679f;
    constexpr float kIgelPitchMin = 0.45f;
    constexpr float kIgelPitchMax = 2.45f;
    // 左右の大型砲は担当する舷側と正面境界で反応する。
    // 正面では左右・中央の3基が揃って発射し、右側では右砲だけ、左側では左砲だけが追尾する。
    for (size_t index = 0; index < mainTurretYaws_.size(); ++index) {
        const bool handlesTarget = index == 0 ? requestedYaw <= 0.08f
            : index == 1 ? requestedYaw >= -0.08f
            : std::abs(requestedYaw) <= 0.62f;
        mainTurretEngaged_[index] = handlesTarget;
        const float desiredRelativeYaw = handlesTarget
            ? std::clamp(WrapAngle(requestedYaw - kMainTurretBaseYaws[index]),
                kMainTurretYawMinimums[index], kMainTurretYawMaximums[index])
            : 0.0f;
        mainTurretYaws_[index] += WrapAngle(desiredRelativeYaw - mainTurretYaws_[index]) *
            tuning_.mainTurretYawFollow;
    }
    // 既存の中央機銃・艦首中央砲は前方半円に限定する。
    const float desiredYaw = std::clamp(requestedYaw, -1.57079632679f, 1.57079632679f);
    turretYaw_ += WrapAngle(desiredYaw - turretYaw_) * tuning_.mainTurretYawFollow;
    turretPitch_ += (requestedPitch - turretPitch_) * tuning_.mainTurretPitchFollow;

    // 上部ミニ連装砲は戦艦の機関砲と同じく、台座が横旋回・砲身が上下旋回する。
    const float igelYawFollow = std::clamp(tuning_.mainTurretYawFollow * 3.0f, 0.16f, 0.48f);
    const float igelPitchFollow = std::clamp(tuning_.mainTurretPitchFollow * 3.0f, 0.16f, 0.48f);
    for (size_t index = 0; index < miniTurretYaws_.size(); ++index) {
        // 各台座自身からプレイヤーを測る。ボス中心を共用しないため、近距離でも左右が同期しない。
        const Vector3 miniPosition = LocalPointToWorld(kMiniTurretBasePivots[index]);
        const Vector3 toPlayer = SubtractVector(playerPos, miniPosition);
        const float miniHorizontalLength = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        if (miniHorizontalLength <= 0.0001f) continue;
        const float miniRequestedYaw = WrapAngle(std::atan2(toPlayer.x, toPlayer.z) - rotation_.y);
        const float miniRequestedPitch = std::clamp(-std::atan2(toPlayer.y, miniHorizontalLength),
            -tuning_.mainTurretMaxPitch, tuning_.mainTurretMaxPitch);
        // 左右の副砲を±90度へ配置し、それぞれ45度ずつの扇形を受け持たせる。
        const float desiredRelativeYaw = std::clamp(
            WrapAngle(miniRequestedYaw - kMiniTurretBaseYaws[index]), -kQuarterPi, kQuarterPi);
        miniTurretYaws_[index] += WrapAngle(desiredRelativeYaw - miniTurretYaws_[index]) * igelYawFollow;
        const float desiredPitch = std::clamp(kIgelForwardPitch + miniRequestedPitch,
            kIgelPitchMin, kIgelPitchMax);
        miniTurretPitches_[index] += (desiredPitch - miniTurretPitches_[index]) * igelPitchFollow;
    }
}

void Boss::UpdateTurretModels() {
    auto updateTurret = [this](Object3d* turret, const Vector3& pivot, float pitch, float yaw,
        float modelPitchOffset = 0.0f) {
        if (!turret) return;
        const Vector3 worldOffset = RotatePitchYaw({
            pivot.x * kBossScale.x, pivot.y * kBossScale.y, pivot.z * kBossScale.z
        }, 0.0f, rotation_.y);
        turret->SetTranslate(AddVector(position_, worldOffset));
        turret->SetRotate({ pitch + modelPitchOffset, rotation_.y + yaw, 0.0f });
        turret->SetScale(kBossScale);
        turret->Update();
    };

    for (size_t index = 0; index < mainTurrets_.size(); ++index) {
        if (IsTurretAlive(kMainTurretFirst + index)) {
            updateTurret(mainTurrets_[index].get(), kMainWeaponMounts[index].pivot,
                0.0f, kMainTurretBaseYaws[index] + mainTurretYaws_[index],
                kMainWeaponMounts[index].modelPitchOffset);
        }
    }
    // ミニ砲台: 箱形の台座だけを横旋回する。
    for (size_t index = 0; index < miniTurretBases_.size(); ++index) {
        if (IsTurretAlive(kMiniTurretFirst + index)) {
            updateTurret(miniTurretBases_[index].get(), kMiniTurretBasePivots[index], 0.0f,
                kMiniTurretBaseYaws[index] + miniTurretYaws_[index]);
        }
    }
    // ミニ砲身: 台座の横旋回に追従しつつ、各自のヒンジで上下へ動く。
    for (size_t index = 0; index < miniTurretBarrels_.size(); ++index) {
        if (!IsTurretAlive(kMiniTurretFirst + index)) continue;
        Object3d* barrel = miniTurretBarrels_[index].get();
        if (!barrel) continue;
        const Vector3& barrelPivot = kMiniTurretBarrelPivots[index];
        const Vector3& basePivot = kMiniTurretBasePivots[index];
        const float yaw = kMiniTurretBaseYaws[index] + miniTurretYaws_[index];
        const Vector3 rotatedBarrelPivot = AddVector(basePivot,
            RotatePitchYaw(SubtractVector(barrelPivot, basePivot), 0.0f, yaw));
        const Vector3 localOffset = RotatePitchYaw({
            rotatedBarrelPivot.x * kBossScale.x, rotatedBarrelPivot.y * kBossScale.y,
            rotatedBarrelPivot.z * kBossScale.z
        }, 0.0f, rotation_.y);
        barrel->SetTranslate(AddVector(position_, localOffset));
        barrel->SetRotate({ miniTurretPitches_[index], rotation_.y + yaw, 0.0f });
        barrel->SetScale(kBossScale);
        barrel->Update();
    }
    // 通常の機銃も横回転だけ。上部ミニ連装砲とは別の砲台として扱う。
    if (IsTurretAlive(kMachineGunTurretIndex)) {
        updateTurret(machineGunTurret_.get(), kMachineGunMount.pivot, 0.0f, turretYaw_,
            kMachineGunMount.modelPitchOffset);
    }
    for (size_t index = 0; index < forwardTurrets_.size(); ++index) {
        if (IsTurretAlive(kForwardTurretFirst + index)) {
            // 左右砲台だけはヨーを固定し、ピッチ回転だけにする。
            const float yaw = index > 0 ? 0.0f : turretYaw_;
            const float pitch = index > 0 ? turretPitch_ : 0.0f;
            updateTurret(forwardTurrets_[index].get(), kForwardWeaponMounts[index].pivot, pitch, yaw);
        }
    }
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

    const Vector3 exhaustDirection = RotatePitchYaw(kRearThrusterDirection, 0.0f, rotation_.y);
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
    UpdateRearThrusters();
    UpdateTurretModels();
}

void Boss::Draw() {
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
    DrawRearThrusters();
}
