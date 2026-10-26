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
class EnemyActionController;

// 敵の行動パターン（状態）を定義
enum class EnemyState {
    Approach, // プレイヤーに近づく
    Attack    // 攻撃する
};

class Enemy {
public:
    virtual ~Enemy() = default;
    static constexpr size_t kNoSpawnPoint = static_cast<size_t>(-1);

    // 初期化（発生位置を渡す）
    virtual void Initialize(const Vector3 &position);

    // 毎フレームの更新
    virtual void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles);

    // 描画
    virtual void Draw();

    // 更新だけしてロジックを動かさない処理（シミュレーション時など用）
    virtual void UpdateModel();

    // ミサイルに狙われるためのゲッター・セッター
    Vector3 GetPosition() const { return position_; }
    virtual void SetPosition(const Vector3 &position) { position_ = position; }
    Vector3 GetRotation() const { return rotation_; }
    Vector3 GetScale() const { return scale_; }

    Vector3 GetWorldHalfExtents() const;
    float GetCollisionRadius() const;
    OBB GetOBB() const;

    // ミサイルと衝突したときの処理
    void OnCollision();
    // hitDirection は攻撃側から被弾点へ向かう単位方向。未指定の近接攻撃ではゼロベクトル。
    virtual void TakeDamage(int damage, const Vector3 &hitDirection = { 0.0f, 0.0f, 0.0f });
    // 撃破演出中など、当たり判定には残るが追加ダメージを受け付けない敵のための問い合わせ。
    virtual bool CanReceiveDamage() const { return !isDead_; }
    void StartChasingPlayer();

    // 死んだかどうかのゲッター
    bool IsDead() const { return isDead_; }
    int GetHP() const { return hp_; }

    // サイズ（スケール）を設定するセッター
    void SetScale(const Vector3 &scale) { scale_ = scale; if (object_) object_->SetScale(scale_); }

    // 回転
    void SetRotation(const Vector3 &rotation);
    void SetFlightPath(const std::vector<Vector3> &points, bool loop, float speed);

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

protected:
	friend class EnemyActionController;
    void UpdateFlightPathAI(const Vector3 &playerPos, EnemyBulletManager *bulletManager);
    void BuildPathVisualizers();

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
    int hp_ = 2;

    // 死んだかどうかのフラグ
    bool isDead_ = false;
    bool isBoss_ = false; // ボスフラグ
    bool isBossMinion_ = false;
    size_t spawnPointIndex_ = kNoSpawnPoint;

    // ルート可視化用
    // デバッグ用の経路球は大量のGPUリソースを生成するため通常は無効。
    inline static bool pathVisualizationEnabled_ = false;
    std::vector<std::unique_ptr<class Primitive>> pathVisualizers_;
};

