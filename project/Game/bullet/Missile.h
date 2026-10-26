#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include "engine/Camera/Camera.h"
#include <memory>
#include <string>


class Enemy;
class ParticleManager;

// 弾の種類を定義
enum class MissileType {
    Normal,         // 真っ直ぐ飛ぶ弾（煙なし）
    MissileWithTrail // 煙を引くミサイル
};

struct MissileTuning {
    float speed = 0.75f;
    float homingStrength = 0.085f;
    float scale = 0.5f;
    float collisionRadius = 0.5f;
    float trailWidth = 0.5f;
    int lifeTime = 240;
};

class Missile {
public:
    // 初期化（発生位置と飛んでいく方向を渡す）
    void Initialize(const Vector3 &position, const Vector3 &velocity, MissileType type, const MissileTuning &tuning, ParticleManager* pManager = nullptr, Enemy* target = nullptr);

    // 毎フレームの更新
    void Update(Camera *camera);

    // 更新だけしてロジックを動かさない処理（シミュレーション時など用）
    void UpdateModel(Camera *camera = nullptr);

    // 描画
    void Draw();

    // 寿命が尽きたかどうか（外部から削除タイミングを判定するため）
    bool IsDead() const { return isDead_; }

    // 当たった時に呼ばれる関数
    void OnCollision();

    // 地表へ潜り込んだ場合、地面すれすれまで戻して上向きに軌道補正する
    void ResolveGroundContact(float terrainY, float clearance);

    // 自分の座標を外に教えるゲッター
    Vector3 GetPosition() const { return position_; }
    Vector3 GetVelocity() const { return velocity_; }
    Enemy* GetTarget() const { return target_; }
    float GetCollisionRadius() const { return collisionRadius_; }
    void ClearTargetIfMatches(const Enemy* target) {
        if (target_ == target) {
            target_ = nullptr;
        }
    }

private:
    std::unique_ptr<Object3d> object_;
    MissileType type_;     // 自分のタイプを保持
    MissileTuning tuning_;

    // このミサイルが煙を出すためのパーティクルマネージャ
    ParticleManager* particleManager_ = nullptr;
    Enemy* target_ = nullptr;

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f }; // 1フレームに進む量
    float collisionRadius_ = 0.3f;

    // うねり軌道用パラメータ
    float phaseOffset_ = 0.0f;
    float waveSign_ = 1.0f;
    float spiralSpeed_ = 0.15f;

    // 寿命（画面外に飛び去った弾をメモリから消すため）
    int lifeTimer_ = 0;
    int elapsedFrames_ = 0;
    static const int kLifeTime = 120; // 120フレーム（約2秒）で消滅
    static const int kHomingLifeTime = 240;
    bool isDead_ = false;
};


