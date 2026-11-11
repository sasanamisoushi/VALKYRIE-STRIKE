#pragma once
#include "Enemy.h"
#include "3D/Trail.h"
#include <array>

// ボス敵を表すクラス
class Boss : public Enemy {
public:
    // 通常弾だけでもボス戦として成立する耐久値にする。
    static constexpr int kMaxHP = 240;
    static constexpr size_t kMaxRearThrusters = 12;
    // 艦首3基、主砲3基、上部連装砲2基、機銃1基を個別に破壊できる。
    static constexpr size_t kTurretCount = 9;

    // ImGui のボス戦調整で使う、砲台・後部スラスターのライブ設定。
    struct Tuning {
        bool mainTurretYawTracking = true;
        bool mainTurretPitchTracking = true;
        float mainTurretYawFollow = 0.09f;
        float mainTurretPitchFollow = 0.09f;
        float mainTurretMaxPitch = 0.52f;

        // 艦首左右砲台も他の砲台と同じくピッチ（上下回転）で照準する。
        // 旧保存データとの互換用に値は残すが、縦スライドは使用しない。
        bool forwardTurretsVerticalSlide = false;
        float forwardTurretSlideAmount = 0.18f;
        float forwardTurretSlideSpeed = 0.065f;
        float forwardTurretAimSlide = 0.62f;

        // 中央・左右パネルの2列×2段、合計12個の穴を既定で使用する。
        int rearThrusterCount = 12;
        Vector3 rearThrusterOffset = { 0.02f, 0.0f, 0.0f };
        float rearThrusterSpacing = 0.62f;
        float rearThrusterRadius = 0.27f;
        float rearThrusterLength = 2.55f;
        float rearThrusterBrightness = 0.90f;
        float rearThrusterFlareRadius = 0.42f;
        // 共通補正とは別に、各スラスターを編集画面から個別に移動できる。
        std::array<Vector3, kMaxRearThrusters> rearThrusterIndividualOffsets{};
    };

    Boss() = default;
    ~Boss() override = default;

    // 初期化をオーバーライドして、ボス用のHPやサイズを設定する
    void Initialize(const Vector3 &position) override;

    // 必要に応じてUpdateやDrawもオーバーライド可能
    void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) override;
    void UpdateModel() override;
    void Draw() override;
    void TakeDamageAt(int damage, const Vector3 &hitPosition,
        const Vector3 &hitDirection = { 0.0f, 0.0f, 0.0f }) override;
    void TakeDamageAtTargetPoint(int damage, const Vector3& targetPoint,
        const Vector3& hitPosition, const Vector3& hitDirection = { 0.0f, 0.0f, 0.0f }) override;
    bool IsHitBy(const Sphere& attackSphere) const override;
    OBB GetOBB() const override;
    int GetMaxHP() const override { return kMaxHP; }
    int ConsumeSummonRequests();
    size_t GetTurretCount() const { return kTurretCount; }
    // 船体モデルの境界中心。Enemy::position_ は OBJ の原点なので本体ロックオンには使わない。
    Vector3 GetBodyLockOnPosition() const;
    Vector3 GetAimPosition() const override { return GetBodyLockOnPosition(); }
    bool IsTurretAlive(size_t turretIndex) const;
    Vector3 GetTurretLockOnPosition(size_t turretIndex) const;
    float GetTurretLockOnRadius(size_t turretIndex) const;
    int GetTurretHP(size_t turretIndex) const;
    int GetTurretMaxHP() const;
    const Tuning& GetTuning() const { return tuning_; }
    void SetTuning(const Tuning& tuning);
    size_t GetRearThrusterCount() const { return static_cast<size_t>(tuning_.rearThrusterCount); }
    Vector3 GetRearThrusterWorldPosition(size_t thrusterIndex) const;

private:
    void FireMachineGun(const Vector3 &playerPos, EnemyBulletManager *bulletManager);
    void FireMainBeam(const Vector3 &playerPos, EnemyBulletManager *bulletManager, size_t turretIndex);
    void FireForwardTurret(const Vector3 &playerPos, EnemyBulletManager *bulletManager, size_t turretIndex);
    Vector3 DirectionTo(const Vector3 &target) const;
    Vector3 LocalPointToWorld(const Vector3 &localPoint) const;
    Vector3 GetTurretPivot(size_t turretIndex) const;
    float GetTurretHitRadius(size_t turretIndex) const;
    bool ApplyTurretDamage(size_t turretIndex, int damage);
    void UpdateTurretAim(const Vector3 &playerPos);
    void UpdateTurretModels();
    void InitializeRearThrusters();
    void UpdateRearThrusters();
    void DrawRearThrusters();

    int actionTimer_ = 0;
    int summonRequests_ = 0;
    float turretYaw_ = 0.0f;
    float turretPitch_ = 0.0f;
    // 砲台ごとに異なる担当方位を持たせる。
    std::array<float, 3> mainTurretYaws_{};
    std::array<bool, 3> mainTurretEngaged_{};
    std::array<float, 2> miniTurretYaws_{};
    std::array<float, 2> miniTurretPitches_{};
    // 大型砲は砲身だけを分離し、砲座と装甲箱を船体に固定する。
    std::array<std::unique_ptr<Object3d>, 3> mainTurrets_;
    // 上部ミニ連装砲は、横旋回する台座と上下旋回する砲身を分離する。
    std::array<std::unique_ptr<Object3d>, 2> miniTurretBases_;
    std::array<std::unique_ptr<Object3d>, 2> miniTurretBarrels_;
    std::unique_ptr<Object3d> machineGunTurret_;
    // 艦首側の中央・左右砲台。
    std::array<std::unique_ptr<Object3d>, 3> forwardTurrets_;
    // すべての砲台で共有する部位耐久値と破壊状態。
    std::array<int, kTurretCount> turretHP_{};
    std::array<bool, kTurretCount> turretDestroyed_{};
    // 後部中央と左右の長方形パネルに対応する、常時点火の推進炎。
    std::array<std::unique_ptr<Trail>, kMaxRearThrusters> rearThrusterTrails_;
    std::array<std::unique_ptr<Object3d>, kMaxRearThrusters> rearThrusterObjects_;
    std::array<std::unique_ptr<Object3d>, kMaxRearThrusters> rearThrusterFlareObjects_;
    std::array<Vector3, kMaxRearThrusters> rearThrusterOrigins_{};
    Tuning tuning_{};
};
