#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <cstddef>
#include <memory>
#include <list>
#include <vector>
#include "3D/Primitive.h"


class Obstacle;


class EnemyBulletManager;

// 敵の行動パターン（状態）を定義
enum class EnemyState {
    Approach, // プレイヤーに近づく
    Attack    // 攻撃する
};

class Enemy {
public:
    virtual ~Enemy() = default;
    static constexpr size_t kNoSpawnPoint = static_cast<size_t>(-1);
    static constexpr int kMaxHP = 2;

    // 初期化（発生位置を渡す）
    virtual void Initialize(const Vector3 &position);

    // スポーン直後に必要な種類固有の配置補正。通常敵は何もしない。
    virtual void OnSpawned(const std::list<std::unique_ptr<Obstacle>>& obstacles) { (void)obstacles; }

    // 毎フレームの更新
    virtual void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles);

    // 描画
    virtual void Draw();

    // 不透明物をすべて描画した後に行う半透明描画用のパス。
    // 通常の敵には半透明の専用部位がないため何もしない。
    virtual void DrawTransparent() {}

    // 更新だけしてロジックを動かさない処理（シミュレーション時など用）
    virtual void UpdateModel();

    // ミサイルに狙われるためのゲッター・セッター
    Vector3 GetPosition() const { return position_; }
    // 誘導弾・カメラが狙う位置。通常敵は本体座標、特殊な原点を持つ敵はオーバーライドする。
    virtual Vector3 GetAimPosition() const { return position_; }
    virtual void SetPosition(const Vector3 &position) { position_ = position; }
    Vector3 GetRotation() const { return rotation_; }
    Vector3 GetScale() const { return scale_; }
    Vector3 GetVelocity() const { return velocity_; }
	// 撃破演出で元の敵モデルを一時的に使うための描画情報。
	Model* GetPresentationModel() const { return object_ ? object_->GetModel() : nullptr; }
	void SetPresentationVisible(bool visible) { presentationVisible_ = visible; }

    virtual Vector3 GetWorldHalfExtents() const;
    virtual float GetCollisionRadius() const;
    virtual OBB GetOBB() const;
    // 通常敵は本体 OBB のみを使う。複数部位を持つ敵はこれを拡張して、
    // 本体から離れた可動部にも弾を当てられる。
    virtual bool IsHitBy(const Sphere& attackSphere) const;

    // ミサイルと衝突したときの処理
    void OnCollision();
    // hitDirection は攻撃側から被弾点へ向かう単位方向。未指定の近接攻撃ではゼロベクトル。
    virtual void TakeDamage(int damage, const Vector3 &hitDirection = { 0.0f, 0.0f, 0.0f });
    // 弾の着弾位置を利用できる敵向け。部位判定を持たない敵は従来どおり本体へダメージを受ける。
    virtual void TakeDamageAt(int damage, const Vector3 &hitPosition,
        const Vector3 &hitDirection = { 0.0f, 0.0f, 0.0f }, float impactRadius = 0.0f);
    // ロックオン点を持つ弾用。通常敵は実際の着弾位置で従来どおり処理する。
    virtual void TakeDamageAtTargetPoint(int damage, const Vector3& targetPoint,
        const Vector3& hitPosition, const Vector3& hitDirection = { 0.0f, 0.0f, 0.0f },
        float impactRadius = 0.0f);
    // 撃破演出中など、当たり判定には残るが追加ダメージを受け付けない敵のための問い合わせ。
    virtual bool CanReceiveDamage() const { return !isDead_; }
    void StartChasingPlayer();

    // 死んだかどうかのゲッター
    bool IsDead() const { return isDead_; }
    int GetHP() const { return hp_; }
    virtual int GetMaxHP() const { return kMaxHP; }

    // サイズ（スケール）を設定するセッター
    void SetScale(const Vector3 &scale) { scale_ = scale; if (object_) object_->SetScale(scale_); }

    // 回転
    void SetRotation(const Vector3 &rotation);
    void SetFlightPath(const std::vector<Vector3> &points, bool loop, float speed);
    // チュートリアル用の標的は、反撃も追尾もしない静止ターゲットとして扱う。
    void SetTrainingTarget(bool enabled) { isTrainingTarget_ = enabled; }

    // 全敵の飛行ルート表示を切り替える（デバッグ表示用）。
    static void SetPathVisualizationEnabled(bool enabled) { pathVisualizationEnabled_ = enabled; }
    static bool IsPathVisualizationEnabled() { return pathVisualizationEnabled_; }

    // Blenderで配置したリスポーン地点との対応
    void SetSpawnPointIndex(size_t index) { spawnPointIndex_ = index; }
    size_t GetSpawnPointIndex() const { return spawnPointIndex_; }

    // 思考・行動処理
    void UpdateAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager); 
   
    // 当たり判定処理
    void CheckCollision(const std::list<std::unique_ptr<Obstacle>> &obstacles); 

    // ボス判定
    bool IsBoss() const { return isBoss_; }
    void SetIsBoss(bool isBoss) { isBoss_ = isBoss; }
    bool IsBossMinion() const { return isBossMinion_; }
    void SetIsBossMinion(bool isBossMinion) { isBossMinion_ = isBossMinion; }

    // ロックオン部位（多部位を持つ敵のみオーバーライド）
    virtual Vector3 GetBodyLockOnPosition() const { return GetPosition(); }
    virtual size_t GetLockOnPartCount() const { return 0; }
    virtual bool IsLockOnPartAvailable(size_t partIndex) const { (void)partIndex; return false; }
    virtual Vector3 GetLockOnPartPosition(size_t partIndex) const { (void)partIndex; return GetAimPosition(); }
    virtual float GetLockOnPartRadius(size_t partIndex) const { (void)partIndex; return GetCollisionRadius(); }
    virtual int GetLockOnPartHP(size_t partIndex) const { (void)partIndex; return GetHP(); }
    virtual int GetLockOnPartMaxHP(size_t partIndex) const { (void)partIndex; return GetMaxHP(); }

    // 妨害フィールド・近接攻撃（該当する敵のみオーバーライド）
    virtual float GetJammingRadius() const { return 0.0f; }
    virtual bool IsMeleeActive() const { return false; }
    virtual OBB GetMeleeBoxOBB() const { return {}; }

protected:
    void UpdateFlightPathAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager);
    void BuildPathVisualizers();

    void MoveBy(const Vector3 &delta);
    void SetVelocity(const Vector3 &velocity) { velocity_ = velocity; }
    void CancelVelocityAlong(const Vector3 &normal);
    void SetRotationAngles(const Vector3 &rotation) { rotation_ = rotation; }
    void SetForward(const Vector3 &forward) { forward_ = forward; }
    const Vector3 &GetForward() const { return forward_; }

    int GetAttackTimer() const { return attackTimer_; }
    void AdvanceAttackTimer(int delta = 1) { attackTimer_ += delta; }
    void ResetAttackTimer(int value = 0) { attackTimer_ = value; }

    void SetHP(int hp);
    void ApplyDamage(int damage);
    void MarkDead() { isDead_ = true; }
    void MarkAlive() { isDead_ = false; }

    bool IsChasingPlayer() const { return isChasingPlayer_; }
    void SetChasingPlayer(bool chasing) { isChasingPlayer_ = chasing; }

    bool HasFlightPathPoints() const { return flightPathPoints_.size() >= 2; }
    bool AdvanceGroundFlightPath(Vector3 &outTargetPos);

    Object3d *GetObject3d() { return object_.get(); }
    const Object3d *GetObject3d() const { return object_.get(); }
    void ResetObject3d(std::unique_ptr<Object3d> object) { object_ = std::move(object); }
    void SyncModelTransform();

private:
    // AI用の変数
    EnemyState state_ = EnemyState::Approach; // 今の状態（最初は「接近」）
    int attackTimer_ = 0;                     // 攻撃の間隔を測るタイマー

    std::unique_ptr<Object3d> object_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };

    // 回転を保持する変数
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 forward_ = { 0.0f, 0.0f, 1.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    float currentSpeed_ = 0.0f;
    float bankAngle_ = 0.0f;
    int flightTimer_ = 0;

    std::vector<Vector3> flightPathPoints_;
    bool hasFlightPath_ = false;
    bool flightPathLoop_ = false;
    float flightPathSpeed_ = 0.05f;
    size_t flightPathSegmentIndex_ = 0;
    float flightPathSegmentT_ = 0.0f;
    bool isChasingPlayer_ = false;
	bool isTrainingTarget_ = false;
    int hp_ = 2;

    // 死んだかどうかのフラグ
    bool isDead_ = false;
	bool presentationVisible_ = true;
    bool isBoss_ = false; // ボスフラグ
    bool isBossMinion_ = false;
    size_t spawnPointIndex_ = kNoSpawnPoint;

    // ルート可視化用
    // デバッグ用の経路球は大量のGPUリソースを生成するため通常は無効。
    inline static bool pathVisualizationEnabled_ = false;
    std::vector<std::unique_ptr<class Primitive>> pathVisualizers_;
};

