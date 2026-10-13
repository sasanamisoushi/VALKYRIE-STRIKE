#pragma once
#include "3D/Object3d.h"
#include "engine/math/MyMath.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class Obstacle {
public:
    // 初期化
    void Initialize(const std::string& modelName, const Vector3& position, const Vector3& rotation = {0.0f, 0.0f, 0.0f}, const Vector3& scale = {1.0f, 1.0f, 1.0f});

    // 更新
    void Update();

    // 描画
    void Draw();

    // ゲッター
    Vector3 GetPosition() const { return position_; }
    Vector3 GetRotation() const { return rotation_; }
    Vector3 GetScale() const { return scale_; }
    bool IsStageBounds() const { return isStageBounds_; }
    Vector3 GetCollisionOffset() const { return collisionOffset_; }
    Vector3 GetCollisionScale() const { return collisionScale_; }
    bool IsCollisionEnabled() const { return isCollisionEnabled_; }
    bool IsUseMeshCollider() const { return useMeshCollider_; }

    // モデルの頂点からワールド座標系の三角形リストを生成して返す（キャッシュ版）
    const std::vector<Triangle>& GetWorldTriangles() const { return worldTriangles_; }
    const std::vector<size_t>& GetNearbyWorldTriangleIndices(const Sphere& sphere) const;
    Vector3 GetWorldHalfExtents() const {
        auto absf = [](float value) { return value < 0.0f ? -value : value; };
        Vector3 absScale = { absf(scale_.x * collisionScale_.x), absf(scale_.y * collisionScale_.y), absf(scale_.z * collisionScale_.z) };

        if (!isStageBounds_ && object_ && object_->GetModel()) {
            Vector3 localHalf = object_->GetModel()->GetHalfExtents();
            return { localHalf.x * absScale.x, localHalf.y * absScale.y, localHalf.z * absScale.z };
        }
        return absScale;
    }

    // 当たり判定用のOBBを取得
    OBB GetOBB() const;

    // セッター
    void SetPosition(const Vector3& position) { position_ = position; }
    void SetRotation(const Vector3& rotation) { rotation_ = rotation; }
    void SetScale(const Vector3& scale) { scale_ = scale; }
    void SetStageBounds(bool isStageBounds) { isStageBounds_ = isStageBounds; }
    void SetCollisionOffset(const Vector3& offset) { collisionOffset_ = offset; }
    void SetCollisionScale(const Vector3& scale) { collisionScale_ = scale; }
    void SetCollisionEnabled(bool enabled) { isCollisionEnabled_ = enabled; }
    void SetUseMeshCollider(bool use) { useMeshCollider_ = use; }

private:
    std::unique_ptr<Object3d> object_;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };
    Vector3 collisionOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 collisionScale_ = { 1.0f, 1.0f, 1.0f };
    bool isStageBounds_ = false;
    bool isCollisionEnabled_ = true;
    bool useMeshCollider_ = false;

    // メッシュコライダー用キャッシュ
    std::vector<Triangle> worldTriangles_;
    // 高密度な地形でも近傍検索が十分に絞り込まれるよう、32 x 32 の空間グリッドを使う。
    static constexpr int kMeshGridDimension = 32;
    std::array<std::vector<size_t>, kMeshGridDimension * kMeshGridDimension> meshTriangleGrid_;
    std::vector<size_t> allTriangleIndices_;
    mutable std::vector<size_t> nearbyTriangleIndices_;
    Vector3 meshGridMin_ = { 0.0f, 0.0f, 0.0f };
    Vector3 meshGridMax_ = { 0.0f, 0.0f, 0.0f };
    bool hasMeshTriangleGrid_ = false;
    Vector3 prevPosition_ = { 0,0,0 };
    Vector3 prevRotation_ = { 0,0,0 };
    Vector3 prevScale_ = { 0,0,0 };

    void UpdateMeshCollider();
    void RebuildMeshTriangleGrid();
};
