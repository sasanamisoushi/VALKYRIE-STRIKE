#pragma once
#include "Enemy.h"
#include <memory>

class JammerEnemy : public Enemy {
public:
    JammerEnemy() = default;
    ~JammerEnemy() override = default;

    void Initialize(const Vector3 &position) override;
    void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) override;
    void Draw() override;
    void UpdateModel() override;

    // ジャミングフィールドの半径を取得
    float GetJammingRadius() const override { return jammingRadius_; }

private:
    float jammingRadius_ = 80.0f; // プレイヤーへの妨害が届く距離
    std::unique_ptr<Object3d> jammingSphere_;
};
