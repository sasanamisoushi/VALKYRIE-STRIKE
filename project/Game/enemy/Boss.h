#pragma once
#include "Enemy.h"

// ボス敵を表すクラス
class Boss : public Enemy {
public:
    static constexpr int kMaxHP = 60;

    Boss() = default;
    ~Boss() override = default;

    // 初期化をオーバーライドして、ボス用のHPやサイズを設定する
    void Initialize(const Vector3 &position) override;

    // 必要に応じてUpdateやDrawもオーバーライド可能
    void Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) override;
    void UpdateModel() override;
    void Draw() override;
    int GetMaxHP() const override { return kMaxHP; }
    int ConsumeSummonRequests();

private:
    void FireCannon(const Vector3 &playerPos, EnemyBulletManager *bulletManager);
    void FireBeam(const Vector3 &playerPos, EnemyBulletManager *bulletManager);
    Vector3 DirectionTo(const Vector3 &target) const;

    int actionTimer_ = 0;
    int summonRequests_ = 0;
};
