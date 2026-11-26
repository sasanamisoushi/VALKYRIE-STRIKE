#include "CollisionManager.h"
#include <cmath>

void CollisionManager::Clear() {
    colliders_.clear();
}

void CollisionManager::AddCollider(Collider *collider) {
    colliders_.push_back(collider);
}

void CollisionManager::CheckAllCollisions() {
    std::list<Collider *>::iterator itA = colliders_.begin();
    for (; itA != colliders_.end(); ++itA) {
        Collider *colA = *itA;

        std::list<Collider *>::iterator itB = itA;
        ++itB;
        for (; itB != colliders_.end(); ++itB) {
            Collider *colB = *itB;

            // 1. 属性とマスクのチェック（当たらない設定ならスキップ）
            if (!colA->CanCollideWith(*colB)) {
                continue;
            }

            // 2. 形状の種類を取得
            ColliderShape shapeA = colA->GetShapeType();
            ColliderShape shapeB = colB->GetShapeType();

            bool isHit = false;

            // ==========================================
            // パターン別：当たり判定の計算ルート
            // ==========================================
            if (shapeA == ColliderShape::SPHERE && shapeB == ColliderShape::SPHERE) {
                // 【 球 vs 球 】
                Vector3 posA = colA->GetWorldPosition();
                Vector3 posB = colB->GetWorldPosition();
                float radA = colA->GetRadius();
                float radB = colB->GetRadius();
                float dx = posB.x - posA.x; float dy = posB.y - posA.y; float dz = posB.z - posA.z;
                if ((dx * dx + dy * dy + dz * dz) <= (radA + radB) * (radA + radB)) {
                    isHit = true;
                }
            } else if (shapeA == ColliderShape::OBB && shapeB == ColliderShape::OBB) {
                // 【 OBB vs OBB 】
                isHit = MyMath::IsCollision(colA->GetOBB(), colB->GetOBB());
            } else if (shapeA == ColliderShape::SPHERE && shapeB == ColliderShape::OBB) {
                // 【 球(A) vs OBB(B) 】
                Sphere sphereA = { colA->GetWorldPosition(), colA->GetRadius() };
                isHit = MyMath::IsCollision(sphereA, colB->GetOBB());
            } else if (shapeA == ColliderShape::OBB && shapeB == ColliderShape::SPHERE) {
                // 【 OBB(A) vs 球(B) 】
                Sphere sphereB = { colB->GetWorldPosition(), colB->GetRadius() };
                isHit = MyMath::IsCollision(sphereB, colA->GetOBB());
            }

            // ==========================================
            // 3. 当たっていたらお互いに通知する
            // ==========================================
            if (isHit) {
                colA->OnCollision(colB);
                colB->OnCollision(colA);
            }
        }
    }
}