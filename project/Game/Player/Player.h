#pragma once
#include "3D/Object3d.h"
#include "engine/Input/Input.h"
#include "engine/Camera/Camera.h"
#include "engine/math/MyMath.h"
#include "3D/Animation.h"
#include <memory>
#include "BoosterEffect.h"
#include <string>
#include <list>
#include <map>
#include <array>
#include <vector>

class Obstacle;
class PlayerMovementController;
class PlayerActionController;

enum class PlayerMode {
    Fighter,  // 高速飛行
    Gerwalk,  // ホバリング
    Battroid  // 人型
};

struct PlayerModeParams {
    float maxMoveSpeed = 0.22f;
    float moveAcceleration = 0.018f;
    float moveDamping = 0.90f;
    float pitchSpeed = 0.015f;
    float yawSpeed = 0.014f;
    float rollSpeed = 0.025f;
    bool canGuard = false;
    bool canMelee = false;
    int maxMultiLock = 4;
    float lockOnAngleDot = 0.0f;
    float maxLockOnDistance = 1000.0f;
};

class Player {
public:
    static constexpr int kMaxHP = 5;

    // 初期化（読み込むモデルの名前を渡す）
    void Initialize(const std::string &modelName);

    // 毎フレームの更新（キーボード入力による移動と回転）
    void Update(const std::list<std::unique_ptr<Obstacle>> &obstacles, const Vector3 *lockOnTarget = nullptr);

    // 描画
    void Draw(Camera* camera = nullptr, bool drawEffects = true);
    // 不透明な背景・地形の描画後に加算／半透明エフェクトを重ねる。
    void DrawEffects(Camera* camera);

    // 更新だけしてロジックを動かさない処理（シミュレーション時など用）
    // 演出用では入力を読まずにモデル・アニメーションだけ更新できる。
	// advanceState を false にすると、編集モード停止中でも描画用の行列だけを更新し、
	// アニメーション・ガード・補間などのゲーム時間は進めない。
	void UpdateModel(bool allowInput = true, bool advanceState = true);

    // カメラへの追従（Debug用のカメラではなく、本番用カメラをプレイヤーの後ろに置く処理）

    
    void UpdateCamera(Camera *camera, const Vector3 *targetPos = nullptr, bool allowInput = true);
    void SyncRotationToLastCameraDirection();

    Vector3 GetPosition() const { return position_; }
    Vector3 GetVelocity() const { return velocity_; }
    Quaternion GetQuaternion() const { return quaternion_; }
    const Object3d* GetObject3d() const { return object_.get(); }
    BoosterEffect* GetBoosterEffect() const { return boosterEffect_.get(); }
    Vector3 GetWorldHalfExtents() const;
    float GetCollisionRadius() const;
    OBB GetOBB() const;
    const std::string& GetModelName() const { return modelName_; }

    Vector3 GetForwardVector() const; // 今向いている方向（ミサイル発射などに使う）
    Vector3 GetAttackDirection() const; // 必殺技中は操作中の照準方向を返す
    // 境界接近警告用ゲッター
    bool IsNearBoundary() const { return isNearBoundary_; }
    float GetBoundaryWarningIntensity() const { return boundaryWarningIntensity_; }
    Vector3 GetBoundaryAlertPosition() const { return boundaryAlertPosition_; }
    Vector3 GetBoundaryAlertNormal() const { return boundaryAlertNormal_; }
    bool IsNearWallBoundary() const { return isNearWallBoundary_; }
    float GetWallBoundaryWarningIntensity() const { return wallBoundaryWarningIntensity_; }
    Vector3 GetWallBoundaryAlertPosition() const { return wallBoundaryAlertPosition_; }
    Vector3 GetWallBoundaryAlertNormal() const { return wallBoundaryAlertNormal_; }
    bool IsNearCeilingBoundary() const { return isNearCeilingBoundary_; }
    float GetCeilingBoundaryWarningIntensity() const { return ceilingBoundaryWarningIntensity_; }
    Vector3 GetCeilingBoundaryAlertPosition() const { return ceilingBoundaryAlertPosition_; }
    Vector3 GetCeilingBoundaryAlertNormal() const { return ceilingBoundaryAlertNormal_; }

    // セッター
    void SetPosition(const Vector3 &position) {
        position_ = position;
        if (object_) {
            object_->SetTranslate(position_);
            object_->Update();
        }
    }
    void SetScale(const Vector3 &scale) { 
        modelScale_ = scale;
        if (object_) object_->SetScale(scale); 
    }
    void SetRotation(const Vector3 &eulerRotation);
    void SetVelocity(const Vector3& velocity) { velocity_ = velocity; }

    // タイトルなど、入力を受け付けない演出専用の更新。
    void UpdatePresentation(const Vector3& position, const Vector3& eulerRotation,
        float speed, bool isBoosting);

    void OnCollision();
    void TakeDamage(int damage);
    bool IsDead() const { return isDead_; }
    int GetHP() const { return hp_; }
    void SetSpecialAttackActive(bool active);
    bool IsSpecialAttackActive() const { return isSpecialAttackActive_; }
    void SetSongActive(bool active);
    bool IsSongActive() const { return isSongActive_; }
    bool IsDodging() const { return dodgeTimer_ > 0; }
    bool IsGuarding() const { return isGuarding_; }
    
    bool IsMeleeAttacking() const { return isMeleeAttacking_; }
    OBB GetMeleeHitbox() const;
    int GetMeleeDamage() const { return 50; } // 近接攻撃の大ダメージ

    // モード関連
    void ChangeMode(PlayerMode newMode);
    PlayerMode GetCurrentMode() const { return currentMode_; }
    const PlayerModeParams& GetModeParams(PlayerMode mode) const { return modeParams_[static_cast<int>(mode)]; }
    void SetModeParams(PlayerMode mode, const PlayerModeParams& params);

    // アニメーション関連デバッグ用ゲッターセッター
    float GetAnimationTime() const { return animationTime_; }
    void SetAnimationTime(float time) { animationTime_ = time; }
    float GetTargetAnimationTime() const { return targetAnimationTime_; }
    void SetTargetAnimationTime(float time) { targetAnimationTime_ = time; }
    float GetAnimationDuration() const { return animationData_.duration; }
    bool IsAnimDebugActive() const { return isAnimDebugActive_; }
    void SetAnimDebugActive(bool active) { isAnimDebugActive_ = active; }

    const Skeleton& GetSkeleton() const { return skeleton_; }

    void SetOverrideAnimation(const Animation* animation) { overrideAnimation_ = animation; }
    const Animation* GetOverrideAnimation() const { return overrideAnimation_; }

    void PlayActionAnimation(const std::string& actionName);
    void StopActionAnimation();

private:
    friend class PlayerMovementController;
    friend class PlayerActionController;

    // Update() からのみ使う内部の更新手順。外部の Scene や UI から
    // プレイヤーの状態を途中で変更できないよう、公開 API には含めない。
    void Move(bool rotationLocked = false);
    void CheckCollision(const std::list<std::unique_ptr<Obstacle>>& obstacles);
    void UpdateLockOnRotation(const Vector3& targetPos);

    void ApplyBattroidProceduralWalk();
    void ApplyGuardPose(float blendWeight);
    void InitializeTransformPlayerModel();
    void UpdateTransformPlayerModel();
    bool IsTransformPlayerModel() const;

    struct TransformPlayerPose {
        Vector3 translate;
        Vector3 scale;
        Quaternion rotate;
    };

    struct TransformPlayerPart {
        std::string name;
        std::unique_ptr<Object3d> object;
        std::array<TransformPlayerPose, 3> poses;
    };

	std::unique_ptr<Object3d> object_;
	TransformPlayerPart transformCore_;
	std::vector<TransformPlayerPart> transformParts_;
	PlayerMode transformFromMode_ = PlayerMode::Fighter;
	float transformModeBlend_ = 1.0f;
	std::unique_ptr<Object3d> guardBarrier_;
	std::unique_ptr<Object3d> guardBarrierRing_;
	std::string modelName_;
	Vector3 modelScale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 currentDrawScale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 targetDrawScale_ = { 1.0f, 1.0f, 1.0f };

    PlayerMode currentMode_ = PlayerMode::Fighter;
    PlayerModeParams modeParams_[3];

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    Quaternion quaternion_ = { 0.0f, 0.0f, 0.0f, 1.0f }; // 単位クォータニオン（無回転）
    float cameraPitch_ = 0.0f;
    Vector3 lastCameraDirection_ = { 0.0f, 0.0f, 1.0f };

    bool isDead_ = false;
    int hp_ = kMaxHP;
    bool isSpecialAttackActive_ = false;
    float specialAttackCameraYaw_ = 0.0f;
    float specialAttackCameraPitch_ = 0.0f;
    bool isSongActive_ = false;
    static constexpr int kDodgeDurationFrames = 16;
    static constexpr int kDodgeCooldownFrames = 30;
    int dodgeTimer_ = 0;
    int dodgeCooldownTimer_ = 0;
    float dodgeDirection_ = 0.0f;
    bool isGuarding_ = false;
    float guardBarrierPulse_ = 0.0f;
    float guardScale_ = 0.0f;
    float guardPoseWeight_ = 0.0f;
    
    bool isMeleeAttacking_ = false;
    int meleeTimer_ = 0;

    Animation animationData_;
    Skeleton skeleton_;
    float animationTime_ = 0.0f;
    float targetAnimationTime_ = 0.0f;
    float battroidWalkTime_ = 0.0f;
    bool enableSkinning_ = false;
    bool isAnimDebugActive_ = false;
    bool isBattroidWalking_ = false;
    const Animation* overrideAnimation_ = nullptr;

    std::map<std::string, Animation> actionAnimations_;
    Animation* currentActionAnim_ = nullptr;
    float actionAnimTime_ = 0.0f;
    bool isPlayingAction_ = false;

    // 境界接近警告用
    bool isNearBoundary_ = false;
    float boundaryWarningIntensity_ = 0.0f;
    Vector3 boundaryAlertPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 boundaryAlertNormal_ = {0.0f, 0.0f, 1.0f};
    bool isNearWallBoundary_ = false;
    float wallBoundaryWarningIntensity_ = 0.0f;
    Vector3 wallBoundaryAlertPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 wallBoundaryAlertNormal_ = {0.0f, 0.0f, 1.0f};
    bool isNearCeilingBoundary_ = false;
    float ceilingBoundaryWarningIntensity_ = 0.0f;
    Vector3 ceilingBoundaryAlertPosition_ = {0.0f, 0.0f, 0.0f};
    Vector3 ceilingBoundaryAlertNormal_ = {0.0f, 1.0f, 0.0f};

    std::unique_ptr<BoosterEffect> boosterEffect_;
};


