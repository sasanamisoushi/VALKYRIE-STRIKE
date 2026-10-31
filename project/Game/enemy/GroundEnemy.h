#pragma once
#include "Game/enemy/Enemy.h"
#include "3D/Model.h"
#include "3D/Animation.h"
#include <array>
#include <memory>
#include <vector>

class GroundEnemy : public Enemy {
public:
    static constexpr int kMaxHP = 3;

    GroundEnemy() = default;
    ~GroundEnemy() override = default;

    void Initialize(const Vector3 &position) override;
    void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) override;
    void Draw() override;
    void UpdateModel() override;
    void UpdateGroundMovement(const Vector3 &playerPos, const std::list<std::unique_ptr<Obstacle>> &obstacles);
    void SnapToGround(const std::list<std::unique_ptr<Obstacle>> &obstacles);
    void UpdateFlightPathMovement();
    void TakeDamage(int damage, const Vector3 &hitDirection = { 0.0f, 0.0f, 0.0f }) override;
    bool CanReceiveDamage() const override;
    int GetMaxHP() const override { return kMaxHP; }

    // Scene 側が粒子・爆発マネージャへ渡す演出要求。敵ロジックを描画実装から分離する。
    struct EffectEvent {
        enum class Type {
            ArmorSpark,
            TrackSkid,
            ChainDetonation,
            TurretMisfire,
            FinalExplosion,
        };
        Type type = Type::ArmorSpark;
        Vector3 position{};
        Vector3 direction{};
    };
    std::vector<EffectEvent> ConsumeEffectEvents();

    // 近接攻撃判定ブロックのOBBを取得（当たりのチェック用）
    OBB GetMeleeBoxOBB() const;
    bool IsMeleeActive() const { return isMeleeActive_; }

private:
    void UpdateAttackAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager);
    void UpdateLocomotionPose();
    void UpdateTerrainPose(const std::list<std::unique_ptr<Obstacle>> &obstacles);
    void UpdateRiggedPose();
    void UpdateTurretPose();
    void UpdateLegsPose();
    void UpdateHitReaction(const std::list<std::unique_ptr<Obstacle>> &obstacles);
    void BeginDestruction(const Vector3 &hitDirection);
    void UpdateDestruction();
    void UpdateDetachedParts();
    void UpdateDamageGlow();
    Vector3 GetLocalPartPosition(const Vector3 &localPosition) const;
    void QueueEffect(EffectEvent::Type type, const Vector3 &position, const Vector3 &direction = { 0.0f, 0.0f, 0.0f });
    float GetFootOffset() const;

    enum class LocomotionMode {
        Quadruped,
        Track
    };

    // 重力とジャンプ
    float velocityY_ = 0.0f;
    float gravity_ = 0.08f;
    bool isGrounded_ = false;
    float groundY_ = 0.0f;
    int jumpTimer_ = 0;
    int jumpInterval_ = 180; // 約3秒に1回ハイジャンプ機会

    // 攻撃関連
    enum class GroundAttackState {
        Idle,
        Missile,
        Gatling,
        Melee
    };
    GroundAttackState attackSubState_ = GroundAttackState::Idle;
    int attackTimer_ = 0;
    int gatlingShotCount_ = 0;
    int gatlingIntervalTimer_ = 0;

    // 近接攻撃（飛び蹴り）フェーズ
    enum class MeleePhase {
        Rising,   // ジャンプ上昇中（HIT判定なし）
        Falling,  // 急降下・飛び蹴り（HIT判定あり！）
        Landing   // 着地硬直
    };
    MeleePhase meleePhase_ = MeleePhase::Rising;
    int meleeLandingTimer_ = 0;

    // 近接攻撃用当たり判定ブロック
    std::unique_ptr<Object3d> meleeBox_;
    std::unique_ptr<Object3d> turret_;
    // OBJでは各脚のフレームが膝をまたぐ一体メッシュになっているため、通常時は
    // 8部品をまとめた脚モデルとして動かす。これにより装甲・関節・足先が離れない。
    std::array<std::unique_ptr<Object3d>, 4> legs_;
    // EnemyR_Rig.gltf のボーン姿勢。各金属部品は単一ボーンへ100%ウェイトで
    // 割り当てられており、関節を曲げてもパーツが伸びたり胴体から外れたりしない。
    bool usesRiggedModel_ = false;
    Skeleton rigSkeleton_{};
    std::vector<QuaternionTransform> rigBindPose_;
    Vector3 meleeBoxPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 meleeBoxScale_ = { 0.35f, 0.35f, 0.50f };
    bool isMeleeActive_ = false;
    int meleeTimer_ = 0;

    // 静的OBJを壊さず、描画変換だけで四足獣らしい重心移動を作る。
    float gaitPhase_ = 0.0f;
    // 静止時に脚が途中姿勢で固まったりバタついたりしないよう、歩行振幅を滑らかに切り替える。
    float legMotionBlend_ = 0.0f;
    float visualHeightOffset_ = 0.0f;
    Vector3 visualRotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 visualScale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 turretTarget_ = { 0.0f, 0.0f, 0.0f };
    LocomotionMode locomotionMode_ = LocomotionMode::Quadruped;
    float trackBlend_ = 0.0f;
    float trackUvOffset_ = 0.0f;
    float terrainPitch_ = 0.0f;
    float terrainRoll_ = 0.0f;
    // 傾斜で最も低くなる脚先が地表を貫かないための、全パーツ共通の描画リフト量。
    float terrainClearance_ = 0.0f;
    // BlenderのY回転（ゲーム内ではZ軸回転）に対応する、砲身の上下角度。
    float turretPitch_ = 0.0f;
    // 前フレームで更新した砲塔の目標との上下角度差。ビームを砲塔が向く前に
    // 発射しないために使用する。
    float turretAimError_ = 0.0f;
    std::array<float, 4> legGroundOffsets_{};

    enum class DestructionState {
        Alive,
        Sliding,
        ChainDetonating,
    };
    DestructionState destructionState_ = DestructionState::Alive;
    int hitStopFrames_ = 0;
    int hitReactionFrames_ = 0;
    float hitLean_ = 0.0f;
    float skidYawVelocity_ = 0.0f;
    Vector3 hitDirection_ = { 0.0f, 0.0f, 1.0f };
    Vector3 skidVelocity_ = { 0.0f, 0.0f, 0.0f };

    int destructionFrames_ = 0;
    bool finalExplosionQueued_ = false;
    bool turretDetached_ = false;
    std::array<bool, 4> legDetached_{};
    Vector3 turretDebrisPosition_{};
    Vector3 turretDebrisVelocity_{};
    Vector3 turretDebrisRotation_{};
    std::array<Vector3, 4> legDebrisPositions_{};
    std::array<Vector3, 4> legDebrisVelocities_{};
    std::array<Vector3, 4> legDebrisRotations_{};
    std::vector<EffectEvent> pendingEffectEvents_;
};
