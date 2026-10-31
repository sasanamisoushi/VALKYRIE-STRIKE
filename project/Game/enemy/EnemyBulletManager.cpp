#include "EnemyBulletManager.h"
#include "3D/ModelManager.h"
#include "3D/Object3dCommon.h"
#include "engine/Camera/Camera.h"
#include "Game/Player/Player.h"
#include "Game/obstacle/Obstacle.h" // 追加
#include <algorithm>
#include <cmath>
#include <string>

namespace {
    // プレイヤーの通常弾と同じ球モデルに合わせた標準サイズ。
    constexpr float kEnemyBulletScale = 0.3f;
    constexpr float kEnemyBulletRadius = 0.3f;

    Quaternion MakeBeamRotation(const Vector3 &velocity) {
        const float lengthSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
        if (lengthSq <= 0.0001f) {
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }
        const float length = std::sqrt(lengthSq);
        const Vector3 direction = { velocity.x / length, velocity.y / length, velocity.z / length };
        const Quaternion pitch = MyMath::MakeAxisAngle(
            { 1.0f, 0.0f, 0.0f }, -std::asin(std::clamp(direction.y, -1.0f, 1.0f)));
        const Quaternion yaw = MyMath::MakeAxisAngle(
            { 0.0f, 1.0f, 0.0f }, std::atan2(direction.x, direction.z));
        return MyMath::Normalize(MyMath::Multiply(yaw, pitch));
    }

    // 三角形との詳細判定の前に、安価なAABB判定で対象外を除外する。
    // 形状そのものの判定は後段で行うため、命中精度は変わらない。
    bool OverlapsTriangleBounds(const Sphere& sphere, const Triangle& triangle) {
        const float minX = (std::min)({ triangle.p[0].x, triangle.p[1].x, triangle.p[2].x });
        const float maxX = (std::max)({ triangle.p[0].x, triangle.p[1].x, triangle.p[2].x });
        const float minY = (std::min)({ triangle.p[0].y, triangle.p[1].y, triangle.p[2].y });
        const float maxY = (std::max)({ triangle.p[0].y, triangle.p[1].y, triangle.p[2].y });
        const float minZ = (std::min)({ triangle.p[0].z, triangle.p[1].z, triangle.p[2].z });
        const float maxZ = (std::max)({ triangle.p[0].z, triangle.p[1].z, triangle.p[2].z });

        return sphere.center.x + sphere.radius >= minX && sphere.center.x - sphere.radius <= maxX &&
               sphere.center.y + sphere.radius >= minY && sphere.center.y - sphere.radius <= maxY &&
               sphere.center.z + sphere.radius >= minZ && sphere.center.z - sphere.radius <= maxZ;
    }
}

void EnemyBulletManager::Initialize() {
    bullets_.clear();
    bullets_.resize(kMaxBullets);
    for (size_t index = 0; index < bullets_.size(); ++index) {
        auto &bullet = bullets_[index];
        bullet.object = std::make_unique<Object3d>();
        bullet.object->Initialize(Object3dCommon::GetInstance());
        bullet.object->SetModel("EnemyBulletSphere");
        bullet.object->SetScale({ kEnemyBulletScale, kEnemyBulletScale, kEnemyBulletScale });
        if (bullet.object->GetModel()) {
            bullet.object->GetModel()->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f }); // 通常弾と同じ黄色
        }
        bullet.isDead = true; // 最初は非アクティブに設定
        bullet.position = { -9999.0f, -9999.0f, -9999.0f }; // 中心(0,0,0)に残らないよう画面外に退避
        bullet.elapsedFrames = 0;
        bullet.phaseOffset = 0.0f;
        bullet.waveSign = 1.0f;
        bullet.spiralSpeed = 0.12f;
        bullet.beamVisualTime = 0.0f;
        bullet.poolIndex = index;
        bullet.isGroundBeam = false;
        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();
    }
}

void EnemyBulletManager::Update(Player *player, std::vector<Vector3> &hitPositions, const std::list<std::unique_ptr<Obstacle>> &obstacles) {
    const bool canHitPlayer = player && !player->IsDead();
    const OBB playerOBB = canHitPlayer ? player->GetOBB() : OBB{};

    for (auto &bullet : bullets_) {
        if (bullet.isDead) {
            // 非アクティブ弾は Draw() されない。再利用時に座標と行列を更新するため、
            // ここでプール全体の Object3d を更新する必要はない。
            continue;
        }

        // 1. 移動処理
        if (bullet.isHoming && player && !player->IsDead()) {
            bullet.elapsedFrames++;
            if (bullet.elapsedFrames >= 15) { // 15フレーム目から本格的に誘導開始
                Vector3 toPlayer = { player->GetPosition().x - bullet.position.x, player->GetPosition().y - bullet.position.y, player->GetPosition().z - bullet.position.z };
                float lenSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z;
                if (lenSq > 0.001f) {
                    float len = std::sqrt(lenSq);
                    Vector3 desiredDir = { toPlayer.x / len, toPlayer.y / len, toPlayer.z / len };
                    Vector3 currentDir = { bullet.velocity.x, bullet.velocity.y, bullet.velocity.z };
                    float currentSpeed = std::sqrt(currentDir.x * currentDir.x + currentDir.y * currentDir.y + currentDir.z * currentDir.z);
                    if (currentSpeed > 0.001f) {
                        currentDir = { currentDir.x / currentSpeed, currentDir.y / currentSpeed, currentDir.z / currentSpeed };
                    } else {
                        currentDir = desiredDir;
                    }

                    // 旋回強度。プレイヤーのミサイルを模倣
                    float actualHomingStrength = 0.04f; // やや緩めにして直線的になるのを防ぐ
                    if (len < 30.0f) {
                        // 近づくにつれて誘導を強くする
                        float t = len / 30.0f;
                        actualHomingStrength = (std::max)(actualHomingStrength, 1.0f - t);
                    }
                    actualHomingStrength = std::clamp(actualHomingStrength, 0.0f, 1.0f);

                    // ベクトルをブレンド
                    Vector3 homingDir = {
                        currentDir.x * (1.0f - actualHomingStrength) + desiredDir.x * actualHomingStrength,
                        currentDir.y * (1.0f - actualHomingStrength) + desiredDir.y * actualHomingStrength,
                        currentDir.z * (1.0f - actualHomingStrength) + desiredDir.z * actualHomingStrength
                    };
                    // 正規化
                    float homingLenSq = homingDir.x * homingDir.x + homingDir.y * homingDir.y + homingDir.z * homingDir.z;
                    if (homingLenSq > 0.0001f) {
                        float homingLen = std::sqrt(homingLenSq);
                        homingDir = { homingDir.x / homingLen, homingDir.y / homingLen, homingDir.z / homingLen };
                    }

                    // うねうね（スパイラル）用の直交ベクトルを計算
                    Vector3 rightVec = MyMath::Normalize(MyMath::Cross(homingDir, { 0.0f, 1.0f, 0.0f }));
                    if (rightVec.x * rightVec.x + rightVec.y * rightVec.y + rightVec.z * rightVec.z < 0.001f) {
                        rightVec = { 1.0f, 0.0f, 0.0f };
                    }
                    Vector3 upVec = MyMath::Normalize(MyMath::Cross(rightVec, homingDir));

                    float theta = (float)bullet.elapsedFrames * bullet.spiralSpeed + bullet.phaseOffset;
                    
                    // 距離や寿命に応じてうねりをブレンド
                    float fade = std::clamp(len / 40.0f, 0.0f, 1.0f);
                    if (bullet.lifeTimer < 60) {
                        fade *= ((float)bullet.lifeTimer / 60.0f);
                    }
                    float amplitude = 0.35f * fade; // うねりの強さ

                    Vector3 wave = {
                        rightVec.x * std::cos(theta) * amplitude + upVec.x * std::sin(theta) * bullet.waveSign * amplitude,
                        rightVec.y * std::cos(theta) * amplitude + upVec.y * std::sin(theta) * bullet.waveSign * amplitude,
                        rightVec.z * std::cos(theta) * amplitude + upVec.z * std::sin(theta) * bullet.waveSign * amplitude
                    };

                    // 進行方向とうねりを合成
                    Vector3 finalDir = {
                        homingDir.x * 2.0f + wave.x,
                        homingDir.y * 2.0f + wave.y,
                        homingDir.z * 2.0f + wave.z
                    };
                    float finalLenSq = finalDir.x * finalDir.x + finalDir.y * finalDir.y + finalDir.z * finalDir.z;
                    if (finalLenSq > 0.0001f) {
                        float finalLen = std::sqrt(finalLenSq);
                        finalDir = { finalDir.x / finalLen, finalDir.y / finalLen, finalDir.z / finalLen };
                    }

                    bullet.velocity.x = finalDir.x * bullet.speed;
                    bullet.velocity.y = finalDir.y * bullet.speed;
                    bullet.velocity.z = finalDir.z * bullet.speed;
                }
            }
        } else if (bullet.isHoming) {
            bullet.elapsedFrames++;
        }

        bullet.position.x += bullet.velocity.x;
        bullet.position.y += bullet.velocity.y;
        bullet.position.z += bullet.velocity.z;

        if (bullet.isGroundBeam && bullet.thrusterTrail) {
            // 弾の移動履歴を使うことで、直進・横移動中のどちらでも
            // スラスター炎が後方へ自然に流れる。
            bullet.beamVisualTime += 0.34f;
            bullet.thrusterTrail->Update(bullet.position);
        }

        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();

        // 2. 寿命チェック
        bullet.lifeTimer--;
        if (bullet.lifeTimer <= 0) {
            bullet.isDead = true;
            continue;
        }

        // 3. 障害物との当たり判定
        bool hitObstacle = false;
        Sphere bulletSphere;
        bulletSphere.center = bullet.position;
        bulletSphere.radius = bullet.collisionRadius;

        for (const auto& obstacle : obstacles) {
            if (!obstacle || obstacle->IsStageBounds() || !obstacle->IsCollisionEnabled()) {
                continue;
            }

            // メッシュ判定に入る前に障害物全体を弾の球と照合する。
            // OBBで命中を確定させず、通過した弾だけ三角形との詳細判定へ進める。
            const OBB obstacleOBB = obstacle->GetOBB();
            if (!MyMath::IsCollision(bulletSphere, obstacleOBB)) {
                continue;
            }

            bool collided = false;
            if (obstacle->IsUseMeshCollider()) {
                // 地形はモデル全体を囲むOBBではなく、実際の三角形の表面だけで判定する。
                // これにより、地上敵(VF3)の弾が発射直後に地形の箱へ触れて消えるのを防ぐ。
                const auto& triangles = obstacle->GetWorldTriangles();
                for (const size_t triangleIndex : obstacle->GetNearbyWorldTriangleIndices(bulletSphere)) {
                    const Triangle& triangle = triangles[triangleIndex];
                    if (!OverlapsTriangleBounds(bulletSphere, triangle)) {
                        continue;
                    }
                    Vector3 pushOut;
                    if (MyMath::IsCollision(bulletSphere, triangle, pushOut)) {
                        collided = true;
                        break;
                    }
                }
            } else {
                collided = true;
            }

            if (collided) {
                bullet.isDead = true;
                hitObstacle = true;
                hitPositions.push_back(bullet.position);
                break;
            }
        }

        if (hitObstacle) {
            continue; // 障害物に当たったら以降の判定はスキップ
        }

        if (canHitPlayer && !player->IsDead()) {
            if (MyMath::IsCollision(bulletSphere, playerOBB)) {
                bullet.isDead = true; // 弾を消す

                // プレイヤーにダメージを与える
                player->TakeDamage(bullet.damage);

                if (!player->IsDead()) {
                    hitPositions.push_back(player->GetPosition());
                }
            }
        }
    }
}

void EnemyBulletManager::UpdateModels() {
    for (auto &bullet : bullets_) {
        if (bullet.isDead || !bullet.object) {
            continue;
        }

        bullet.object->SetTranslate(bullet.position);
        bullet.object->Update();
    }
}

void EnemyBulletManager::Draw(Camera *camera) {
    if (!camera) {
        for (auto &bullet : bullets_) {
            if (!bullet.isDead && bullet.object) {
                bullet.object->Draw();
            }
        }
        return;
    }

    Vector4 frustumPlanes[6];
    MyMath::ExtractFrustumPlanes(camera->GetViewProjectionMatrix(), frustumPlanes);
    const Vector3 cameraPosition = camera->GetTranslate();

    // 距離順にして、弾が大量にある場合でもプレイヤーの近くにある弾を優先して描画する。
    std::array<std::pair<float, Bullet *>, kMaxBullets> visibleBullets{};
    size_t visibleCount = 0;
    for (auto &bullet : bullets_) {
        if (bullet.isDead || !bullet.object) {
            continue;
        }

        const Sphere bulletSphere{ bullet.position, bullet.collisionRadius };
        if (!MyMath::IsInFrustum(bulletSphere, frustumPlanes)) {
            continue;
        }

        const float dx = bullet.position.x - cameraPosition.x;
        const float dy = bullet.position.y - cameraPosition.y;
        const float dz = bullet.position.z - cameraPosition.z;
        visibleBullets[visibleCount++] = { dx * dx + dy * dy + dz * dz, &bullet };
    }

    const size_t drawCount = (std::min)(visibleCount, kMaxVisibleBullets);
    if (visibleCount > drawCount) {
        std::nth_element(
            visibleBullets.begin(),
            visibleBullets.begin() + static_cast<std::ptrdiff_t>(drawCount),
            visibleBullets.begin() + static_cast<std::ptrdiff_t>(visibleCount),
            [](const auto &left, const auto &right) { return left.first < right.first; });
    }

    for (size_t index = 0; index < drawCount; ++index) {
        visibleBullets[index].second->object->Draw();
    }

    // 地上ビームの後端へ、プレイヤー機と同じ三層のスラスター炎を重ねる。
    Object3dCommon *object3dCommon = Object3dCommon::GetInstance();
    object3dCommon->SetThrusterDrawSettings();
    for (size_t index = 0; index < drawCount; ++index) {
        Bullet *bullet = visibleBullets[index].second;
        if (!bullet->isGroundBeam || !bullet->thrusterTrail || !bullet->thrusterObject || !bullet->thrusterFlareObject ||
            !bullet->thrusterObject->GetModel() || !bullet->thrusterFlareObject->GetModel()) {
            continue;
        }

        // 炎は三角形リスト、フレアは三角形ストリップ用の描画設定を使う。
        object3dCommon->SetThrusterDrawSettings();

        const float speedSq = bullet->velocity.x * bullet->velocity.x + bullet->velocity.y * bullet->velocity.y +
            bullet->velocity.z * bullet->velocity.z;
        const float inverseSpeed = speedSq > 0.0001f ? 1.0f / std::sqrt(speedSq) : 1.0f;
        const Vector3 exhaustDirection = {
            -bullet->velocity.x * inverseSpeed,
            -bullet->velocity.y * inverseSpeed,
            -bullet->velocity.z * inverseSpeed,
        };
        const std::vector<VertexData> flameVertices = bullet->thrusterTrail->GenerateThrusterVertices(
            camera, 0.34f, exhaustDirection, 2.60f, bullet->beamVisualTime, 0.96f, 0.52f, 1.15f);
        if (!flameVertices.empty()) {
            bullet->thrusterObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
            bullet->thrusterObject->SetScale({ 1.0f, 1.0f, 1.0f });
            bullet->thrusterObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
            bullet->thrusterObject->GetModel()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            bullet->thrusterObject->GetModel()->UpdateTrailVertices(flameVertices, false);
            bullet->thrusterObject->Update(camera);
            bullet->thrusterObject->Draw();
        }

        const std::vector<VertexData> flareVertices = bullet->thrusterTrail->GenerateThrusterFlareVertices(
            camera, bullet->position, 0.54f, 0.78f);
        if (!flareVertices.empty()) {
            object3dCommon->SetSpeedTrailDrawSettings();
            bullet->thrusterFlareObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
            bullet->thrusterFlareObject->SetScale({ 1.0f, 1.0f, 1.0f });
            bullet->thrusterFlareObject->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
            bullet->thrusterFlareObject->GetModel()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            bullet->thrusterFlareObject->GetModel()->UpdateTrailVertices(flareVertices, true);
            bullet->thrusterFlareObject->Update(camera);
            bullet->thrusterFlareObject->Draw();
        }
    }
    // 後続の敵モデルは通常の不透明パスで描画する。
    object3dCommon->SetCommonDrawSettings();
}

void EnemyBulletManager::Shoot(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { kEnemyBulletScale, kEnemyBulletScale, kEnemyBulletScale },
                    kEnemyBulletRadius, 1, 120, "EnemyBulletSphere");
}

void EnemyBulletManager::ShootHeavyCannon(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { 1.2f, 1.2f, 2.8f }, 1.2f, 2, 240, "BossCannon");
}

void EnemyBulletManager::ShootBeam(const Vector3 &position, const Vector3 &velocity) {
    ShootConfigured(position, velocity, { 3.5f, 3.5f, 14.0f }, 3.5f, 3, 150, "BossBeam");
}

void EnemyBulletManager::ShootGroundBeam(const Vector3 &position, const Vector3 &velocity) {
    // ラゴゥのビームのように、砲塔が発射時に向いた角度のまま直進する。
    // 丸いシアンの芯の後端には、プレイヤーのスラスター形状の発光炎を重ねる。
    ShootConfigured(position, velocity, { 0.30f, 0.30f, 5.6f }, 0.60f, 1, 105, "GroundBeamCore", true);
}

void EnemyBulletManager::ShootMissile(const Vector3 &position, const Vector3 &velocity) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) { // 使用可能な弾を検索
            bullet.position = position;
            
            // 初期の拡散方向を計算する
            float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
            float speed = (speedSq > 0.0001f) ? std::sqrt(speedSq) : 0.35f;
            Vector3 forward = { velocity.x / speed, velocity.y / speed, velocity.z / speed };
            
            Vector3 right = MyMath::Normalize(MyMath::Cross(forward, { 0.0f, 1.0f, 0.0f }));
            if (right.x * right.x + right.y * right.y + right.z * right.z < 0.001f) {
                right = { 1.0f, 0.0f, 0.0f };
            }
            Vector3 up = MyMath::Normalize(MyMath::Cross(right, forward));
            
            float spreadX = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
            float spreadY = ((float)(rand() % 100) / 100.0f - 0.5f) * 2.0f;
            
            // 初速に対して、横・上への広がりを与える
            Vector3 initialVel = velocity;
            initialVel.x += right.x * spreadX * 0.12f + up.x * spreadY * 0.12f;
            initialVel.y += right.y * spreadX * 0.12f + up.y * spreadY * 0.12f;
            initialVel.z += right.z * spreadX * 0.12f + up.z * spreadY * 0.12f;
            
            bullet.velocity = initialVel;
            bullet.lifeTimer = 240; // 4秒
            bullet.collisionRadius = 0.5f;
            bullet.damage = 1;
            bullet.isHoming = true;
            bullet.isGroundBeam = false;
            bullet.beamVisualTime = 0.0f;
            
            float newSpeedSq = initialVel.x * initialVel.x + initialVel.y * initialVel.y + initialVel.z * initialVel.z;
            bullet.speed = (newSpeedSq > 0.0001f) ? std::sqrt(newSpeedSq) : 0.35f;
            
            bullet.elapsedFrames = 0;
            bullet.phaseOffset = (float)(rand() % 360) * 0.0174532925f;
            bullet.waveSign = (rand() % 2 == 0) ? 1.0f : -1.0f;
            bullet.spiralSpeed = 0.12f + (float)(rand() % 8) * 0.01f;
            
            bullet.isDead = false;

            // プレイヤーの誘導弾と同じ Sphere モデル・赤色・等方スケールに統一する。
            bullet.object->SetModel("EnemyBulletSphere");
            bullet.object->SetScale({ 0.5f, 0.5f, 0.5f });
            if (bullet.object->GetModel()) {
                bullet.object->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // プレイヤー誘導弾と同じ赤色
            }
            bullet.object->SetTranslate(position);
            bullet.object->Update();
            break;
        }
    }
}

void EnemyBulletManager::ShootConfigured(const Vector3 &position, const Vector3 &velocity, const Vector3 &scale,
                                         float collisionRadius, int damage, int lifeTimer, const char *modelName,
                                         bool isGroundBeam) {
    for (auto &bullet : bullets_) {
        if (bullet.isDead) { // 使用可能な弾を検索
            bullet.position = position;
            bullet.velocity = velocity;
            bullet.lifeTimer = lifeTimer;
            bullet.collisionRadius = collisionRadius;
            bullet.damage = damage;
            bullet.isHoming = false;
            bullet.isGroundBeam = isGroundBeam;
            bullet.elapsedFrames = 0;
            bullet.beamVisualTime = 0.0f;
            const float speedSq = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
            bullet.speed = speedSq > 0.0001f ? std::sqrt(speedSq) : 0.72f;
            bullet.isDead = false;

            bullet.object->SetModel(modelName);
            bullet.object->SetScale(scale);
            if (std::string(modelName) == "BossBeam" || std::string(modelName) == "GroundBeamCore") {
                bullet.object->SetQuaternionRotate(MakeBeamRotation(velocity));
            } else {
                bullet.object->SetQuaternionRotate({ 0.0f, 0.0f, 0.0f, 1.0f });
            }
            if (bullet.object->GetModel()) {
                if (std::string(modelName) == "EnemyBulletSphere") {
                    bullet.object->GetModel()->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f });
                } else if (std::string(modelName) == "GroundBeamCore") {
                    bullet.object->GetModel()->SetColor({ 0.10f, 0.90f, 1.0f, 1.0f });
                }
            }
            bullet.object->SetTranslate(position);
            bullet.object->Update();
            if (bullet.isGroundBeam) {
                EnsureGroundBeamVisual(bullet);
                bullet.thrusterTrail->SetOrigin(position);
            }
            break; // 発射したので終了
        }
    }
}

void EnemyBulletManager::EnsureGroundBeamVisual(Bullet &bullet) {
    if (bullet.thrusterTrail && bullet.thrusterObject && bullet.thrusterFlareObject) {
        return;
    }

    // トレイルの頂点データはモデルごとに保持されるため、プール番号ごとの専用モデルにする。
    const std::string modelSuffix = std::to_string(bullet.poolIndex);
    ModelManager *modelManager = ModelManager::GetInstance();
    const std::string flameModelName = "GroundBeamThrusterFlame_" + modelSuffix;
    const std::string flareModelName = "GroundBeamThrusterFlare_" + modelSuffix;
    modelManager->CreateTrailModel(flameModelName);
    modelManager->CreateTrailModel(flareModelName);

    bullet.thrusterTrail = std::make_unique<Trail>();
    bullet.thrusterTrail->Initialize(32);
    bullet.thrusterObject = std::make_unique<Object3d>();
    bullet.thrusterObject->Initialize(Object3dCommon::GetInstance());
    bullet.thrusterObject->SetModel(flameModelName);
    bullet.thrusterFlareObject = std::make_unique<Object3d>();
    bullet.thrusterFlareObject->Initialize(Object3dCommon::GetInstance());
    bullet.thrusterFlareObject->SetModel(flareModelName);
}


