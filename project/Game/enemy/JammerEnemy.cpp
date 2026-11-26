#include "JammerEnemy.h"
#include "3D/Object3dCommon.h"
#include "Game/enemy/EnemyBulletManager.h"
#include "Game/obstacle/Obstacle.h"

void JammerEnemy::Initialize(const Vector3 &position) {
    Enemy::Initialize(position);

    // 見た目を少し変える（黄色にするなど）
    if (Object3d* obj = GetObject3d(); obj && obj->GetModel()) {
        obj->GetModel()->SetColor({ 0.8f, 0.8f, 0.1f, 1.0f });
    }

    // ジャミングフィールドの球体エフェクトを初期化
    jammingSphere_ = std::make_unique<Object3d>();
    jammingSphere_->Initialize(Object3dCommon::GetInstance());
    jammingSphere_->SetModel("sphere.obj");
    if (jammingSphere_->GetModel()) {
        jammingSphere_->GetModel()->SetColor({ 0.1f, 0.8f, 0.8f, 0.3f }); // 半透明なシアン
    }
    // 半径に合わせてスケールを設定
    jammingSphere_->SetScale({ jammingRadius_, jammingRadius_, jammingRadius_ });
    jammingSphere_->SetTranslate(GetPosition());
    jammingSphere_->Update();
}

void JammerEnemy::Update(const Vector3 &playerPos, EnemyBulletManager *bulletManager, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    // 自らは攻撃しないため、タイマーをリセットし続けるか基底クラスのUpdate後に処理
    Enemy::Update(playerPos, bulletManager, obstacles);
    
    // 攻撃タイマーをリセットし続けることで弾を撃たなくする
    ResetAttackTimer(0);

    // ジャミングフィールドの位置を敵に追従させる
    if (jammingSphere_) {
        jammingSphere_->SetTranslate(GetPosition());
        jammingSphere_->Update();
    }
}

void JammerEnemy::Draw() {
    Enemy::Draw();

    if (jammingSphere_ && !IsDead()) {
        jammingSphere_->Draw();
    }
}

void JammerEnemy::UpdateModel() {
    Enemy::UpdateModel();
    if (jammingSphere_) {
        jammingSphere_->SetTranslate(GetPosition());
        jammingSphere_->Update();
    }
}
