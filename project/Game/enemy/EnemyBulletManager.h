#pragma once
#include "3D/Object3d.h"
#include "3D/Trail.h"
#include "engine/math/MyMath.h"
#include <vector>
#include <memory>
#include <list>
#include <array>

class Player; // 前方宣言
class Obstacle; // 前方宣言
class Camera;

class EnemyBulletManager {
public:
    void Initialize();
    // プレイヤーへのポインタを受け取って当たり判定を行う
    void Update(Player *player, std::vector<Vector3> &hitPositions, const std::list<std::unique_ptr<Obstacle>> &obstacles);
    void UpdateModels();
    // 弾幕時は描画数を制限し、命中判定とは独立して描画負荷だけを抑える。
    void Draw(Camera *camera = nullptr);

    // 弾を発射する
    void Shoot(const Vector3 &position, const Vector3 &velocity);
    void ShootHeavyCannon(const Vector3 &position, const Vector3 &velocity);
    void ShootBeam(const Vector3 &position, const Vector3 &velocity);
    void ShootGroundBeam(const Vector3 &position, const Vector3 &velocity);
    void ShootMissile(const Vector3 &position, const Vector3 &velocity);

    // 弾1発のデータ構造
    struct Bullet {
        std::unique_ptr<Object3d> object;
        std::unique_ptr<Trail> thrusterTrail;
        std::unique_ptr<Object3d> thrusterObject;
        std::unique_ptr<Object3d> thrusterFlareObject;
        Vector3 position;
        Vector3 velocity;
        bool isDead = true; // デフォルトは非アクティブ
        int lifeTimer = 120; // 120フレーム（2秒）で自然消滅
        float collisionRadius = 0.2f;
        int damage = 1;
        bool isHoming = false;
        float speed = 0.25f;
        int elapsedFrames = 0;
        float phaseOffset = 0.0f;
        float waveSign = 1.0f;
        float spiralSpeed = 0.12f;
        float beamVisualTime = 0.0f;
        size_t poolIndex = 0;
        bool isGroundBeam = false;
    };

    // デバッグ表示用のゲッター
    const std::vector<Bullet>& GetBullets() const { return bullets_; }

private:
    void ShootConfigured(const Vector3 &position, const Vector3 &velocity, const Vector3 &scale,
                         float collisionRadius, int damage, int lifeTimer, const char *modelName,
                         bool isGroundBeam = false);
    void EnsureGroundBeamVisual(Bullet &bullet);
    static const size_t kMaxBullets = 200; // 弾の最大プール数
    static const size_t kMaxVisibleBullets = 96;
    std::vector<Bullet> bullets_;
};
