#pragma once
#include "Game/bullet/Missile.h"
#include "engine/Particle/ParticleManager.h"
#include "engine/Camera/Camera.h"
#include <list>
#include <memory>
#include <vector>

class Enemy;
class Obstacle; // 追加
class MissileManager {
public:
    // 初期化（リストを空にするなど）
    void Initialize(ParticleManager* particleManager);

    // 全ミサイルの更新と、死んだミサイルの削除
    void Update(Camera *camera, std::list<std::unique_ptr<Enemy>> &enemies, const std::list<std::unique_ptr<Obstacle>> &obstacles, std::vector<Vector3> &hitPositions, std::vector<Vector3> &destroyedPositions);

    void UpdateModels(Camera *camera = nullptr);

    // 全ミサイルの描画
    void Draw();

    // ミサイルを発射する（GamePlaySceneから呼ばれる）
    void Shoot(const Vector3 &position, const Vector3 &velocity, MissileType type, const MissileTuning &tuning,
        Enemy* target = nullptr, const Vector3* targetPoint = nullptr);

    // デバッグ表示用のゲッター
    const std::list<std::unique_ptr<Missile>>& GetMissiles() const { return missiles_; }

    // 敵が破棄される前に、追尾中のミサイルから対象参照を外す。
    void ClearTarget(const Enemy* target);
    void ClearAllTargets();

private:
    // ミサイルの実体を管理するリスト
    std::list<std::unique_ptr<Missile>> missiles_;
    ParticleManager* particleManager_ = nullptr;
};

