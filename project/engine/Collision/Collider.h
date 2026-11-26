#pragma once
#include <cstdint>
#include "MyMath.h"

// 当たり判定の形状（シェイプ）の種類
enum class ColliderShape {
    SPHERE, // 球体
    OBB     // 有向境界ボックス
};

class Collider {
public:
    virtual ~Collider() = default;

    void SetCollisionAttribute(uint32_t attribute) { collisionAttribute_ = attribute; }
    void SetCollisionMask(uint32_t mask) { collisionMask_ = mask; }
    uint32_t GetCollisionAttribute() const { return collisionAttribute_; }
    uint32_t GetCollisionMask() const { return collisionMask_; }
    bool CanCollideWith(const Collider& other) const {
        return (collisionAttribute_ & other.collisionMask_) != 0 &&
               (other.collisionAttribute_ & collisionMask_) != 0;
    }

    // 衝突した瞬間に呼ばれる関数（派生クラスで具体的な処理を書く）
    virtual void OnCollision(Collider *other) = 0;

    // =====================================
       // 形状情報の設定と取得
       // =====================================

       // 自分の形状がどちらなのかを返す（デフォルトは球体にしておく）
    virtual ColliderShape GetShapeType() const { return ColliderShape::SPHERE; }

    // [Sphere用] の情報
    virtual Vector3 GetWorldPosition() const { return { 0,0,0 }; }
    virtual float GetRadius() const { return 1.0f; }

    // [OBB用] の情報（派生クラスでオーバーライドして使います）
    virtual OBB GetOBB() const { return OBB(); }

private:
    // 衝突属性（自分は何者か。デフォルトは全てに当たる）
    uint32_t collisionAttribute_ = 0xFFFFFFFF;
    // 衝突マスク（誰と当たるか。デフォルトは全てと当たる）
    uint32_t collisionMask_ = 0xFFFFFFFF;
};

