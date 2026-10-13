#include "Obstacle.h"
#include "3D/Object3dCommon.h"
#include "3D/ModelManager.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

void Obstacle::Initialize(const std::string& modelName, const Vector3& position, const Vector3& rotation, const Vector3& scale) {
    object_ = std::make_unique<Object3d>();
    object_->Initialize(Object3dCommon::GetInstance());

    if (modelName.find("StageBounds") != std::string::npos) {
        isStageBounds_ = true;

        const std::string wireModelName = "StageBoundsWire";
        ModelManager::GetInstance()->CreateLineModel(wireModelName);

        if (Model* wireModel = ModelManager::GetInstance()->FindModel(wireModelName)) {
            std::vector<VertexData> vertices;
            vertices.reserve(24);

            Vector4 color = { 0.0f, 1.0f, 1.0f, 1.0f };
            Vector3 corners[] = {
                { -1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f },
                {  1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
                { -1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f },
                {  1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f },
            };

            auto addLine = [&](int start, int end) {
                VertexData v1{};
                VertexData v2{};
                v1.position = { corners[start].x, corners[start].y, corners[start].z, 1.0f };
                v2.position = { corners[end].x, corners[end].y, corners[end].z, 1.0f };
                v1.color = color;
                v2.color = color;
                vertices.push_back(v1);
                vertices.push_back(v2);
            };

            addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
            addLine(4, 5); addLine(5, 6); addLine(6, 7); addLine(7, 4);
            addLine(0, 4); addLine(1, 5); addLine(2, 6); addLine(3, 7);
            wireModel->UpdateLineVertices(vertices);
        }

        object_->SetModel(wireModelName);
    } else if (ModelManager::GetInstance()->FindModel(modelName) != nullptr) {
        object_->SetModel(modelName);
    } else {
        object_->SetModel("ObstacleBox");
    }

    // 地形は鏡面反射・環境マップを持たないマットなマテリアルとして描画する。
    const bool isTerrainModel =
        modelName.find("Terrain") != std::string::npos ||
        modelName.find("terrain") != std::string::npos;
    if (isTerrainModel) {
        object_->SetEnvironmentMapEnabled(false);
        if (Model* terrainModel = object_->GetModel()) {
            terrainModel->SetShininess(0.0f);
        }
    }

    position_ = position;
    rotation_ = rotation;
    scale_ = scale;

    object_->SetTranslate(position_);
    object_->SetRotate(rotation_);
    object_->SetScale(scale_);
    object_->Update();
}

void Obstacle::Update() {
    if (object_) {
        object_->SetTranslate(position_);
        object_->SetRotate(rotation_);
        object_->SetScale(scale_);
        object_->Update();
    }
    UpdateMeshCollider();
}

void Obstacle::Draw() {
    // StageBounds は移動範囲とスポーン判定専用の不可視コライダー。
    // ここを描画すると、タイトル・ゲーム画面の両方に水色の枠線が出てしまう。
    if (!object_ || isStageBounds_) {
        return;
    }
    object_->Draw();
}

OBB Obstacle::GetOBB() const {
    OBB obb;
    obb.center = position_;
    obb.orientations[0] = { 1.0f, 0.0f, 0.0f };
    obb.orientations[1] = { 0.0f, 1.0f, 0.0f };
    obb.orientations[2] = { 0.0f, 0.0f, 1.0f };
    obb.size = GetWorldHalfExtents();

    if (object_ && object_->GetModel()) {
        Vector3 localCenter = object_->GetModel()->GetBoundsCenter();
        Vector3 scaledCenter = { 
            (localCenter.x + collisionOffset_.x) * scale_.x, 
            (localCenter.y + collisionOffset_.y) * scale_.y, 
            (localCenter.z + collisionOffset_.z) * scale_.z 
        };
        Vector3 rotatedCenter = MyMath::Transform(scaledCenter, MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::Multiply(MyMath::MakeRotateYMatrix(rotation_.y), MyMath::MakeRotateZMatrix(rotation_.z))));
        obb.center = { position_.x + rotatedCenter.x, position_.y + rotatedCenter.y, position_.z + rotatedCenter.z };

        Matrix4x4 rotMat = MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::Multiply(MyMath::MakeRotateYMatrix(rotation_.y), MyMath::MakeRotateZMatrix(rotation_.z)));
        obb.orientations[0] = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
        obb.orientations[1] = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
        obb.orientations[2] = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
    }
    return obb;
}

const std::vector<size_t>& Obstacle::GetNearbyWorldTriangleIndices(const Sphere& sphere) const {
    if (!hasMeshTriangleGrid_) {
        return allTriangleIndices_;
    }

    const float extentX = meshGridMax_.x - meshGridMin_.x;
    const float extentZ = meshGridMax_.z - meshGridMin_.z;
    const float sphereMinX = sphere.center.x - sphere.radius;
    const float sphereMaxX = sphere.center.x + sphere.radius;
    const float sphereMinZ = sphere.center.z - sphere.radius;
    const float sphereMaxZ = sphere.center.z + sphere.radius;

    nearbyTriangleIndices_.clear();
    if (sphereMaxX < meshGridMin_.x || sphereMinX > meshGridMax_.x ||
        sphereMaxZ < meshGridMin_.z || sphereMinZ > meshGridMax_.z) {
        return nearbyTriangleIndices_;
    }

    const auto toCell = [](float coordinate, float minimum, float extent) {
        const float normalized = (coordinate - minimum) / extent;
        return std::clamp(static_cast<int>(std::floor(normalized * kMeshGridDimension)), 0, kMeshGridDimension - 1);
    };
    const int minCellX = toCell(sphereMinX, meshGridMin_.x, extentX);
    const int maxCellX = toCell(sphereMaxX, meshGridMin_.x, extentX);
    const int minCellZ = toCell(sphereMinZ, meshGridMin_.z, extentZ);
    const int maxCellZ = toCell(sphereMaxZ, meshGridMin_.z, extentZ);

    for (int cellZ = minCellZ; cellZ <= maxCellZ; ++cellZ) {
        for (int cellX = minCellX; cellX <= maxCellX; ++cellX) {
            const auto& cellTriangles = meshTriangleGrid_[static_cast<size_t>(cellZ * kMeshGridDimension + cellX)];
            nearbyTriangleIndices_.insert(nearbyTriangleIndices_.end(), cellTriangles.begin(), cellTriangles.end());
        }
    }
    return nearbyTriangleIndices_;
}

void Obstacle::UpdateMeshCollider() {
    if (!useMeshCollider_) return;
    
    if (prevPosition_.x == position_.x && prevPosition_.y == position_.y && prevPosition_.z == position_.z &&
        prevRotation_.x == rotation_.x && prevRotation_.y == rotation_.y && prevRotation_.z == rotation_.z &&
        prevScale_.x == scale_.x && prevScale_.y == scale_.y && prevScale_.z == scale_.z && 
        !worldTriangles_.empty()) {
        return;
    }

    worldTriangles_.clear();
    if (!object_ || !object_->GetModel()) return;
    
    const auto& modelData = object_->GetModel()->GetModelData();
    if (modelData.vertices.empty()) return;

    Matrix4x4 rotMat = MyMath::Multiply(MyMath::Multiply(MyMath::MakeRoteXMatrix(rotation_.x), MyMath::MakeRotateYMatrix(rotation_.y)), MyMath::MakeRotateZMatrix(rotation_.z));
    Matrix4x4 worldMatrix = MyMath::Multiply(MyMath::Multiply(MyMath::MkeScaleMatrix(scale_), rotMat), MyMath::MakeTranslateMatrix(position_));

    auto getTransformedPos = [&](const Vector4& localPos) -> Vector3 {
        Vector3 v = { localPos.x, localPos.y, localPos.z };
        return MyMath::Transform(v, worldMatrix);
    };

    // OBJ は左手系へ変換する際に X 座標だけを反転しているため、面の頂点順から
    // 再計算した法線がモデル法線と逆向きになる場合がある。接触判定用法線を
    // 頂点法線と同じ半球へ揃え、地表を裏面として扱わないようにする。
    auto alignNormalToVertexNormals = [&](Triangle& triangle, const VertexData& v0, const VertexData& v1, const VertexData& v2) {
        Vector3 localNormal = {
            v0.normal.x + v1.normal.x + v2.normal.x,
            v0.normal.y + v1.normal.y + v2.normal.y,
            v0.normal.z + v1.normal.z + v2.normal.z
        };
        if (MyMath::Length(localNormal) <= 0.00001f) {
            return;
        }

        const float safeScaleX = std::abs(scale_.x) > 0.00001f ? scale_.x : 1.0f;
        const float safeScaleY = std::abs(scale_.y) > 0.00001f ? scale_.y : 1.0f;
        const float safeScaleZ = std::abs(scale_.z) > 0.00001f ? scale_.z : 1.0f;
        localNormal = {
            localNormal.x / safeScaleX,
            localNormal.y / safeScaleY,
            localNormal.z / safeScaleZ
        };
        const Vector3 worldVertexNormal = MyMath::Normalize(MyMath::Transform(localNormal, rotMat));
        if (MyMath::Dot(triangle.normal, worldVertexNormal) < 0.0f) {
            triangle.normal = MyMath::Multiply(-1.0f, triangle.normal);
        }
    };

    if (!modelData.indices.empty()) {
        for (size_t i = 0; i < modelData.indices.size(); i += 3) {
            const VertexData& v0 = modelData.vertices[modelData.indices[i]];
            const VertexData& v1 = modelData.vertices[modelData.indices[i + 1]];
            const VertexData& v2 = modelData.vertices[modelData.indices[i + 2]];
            Triangle t;
            t.p[0] = getTransformedPos(v0.position);
            t.p[1] = getTransformedPos(v1.position);
            t.p[2] = getTransformedPos(v2.position);
            Vector3 edge1 = MyMath::Subtract(t.p[1], t.p[0]);
            Vector3 edge2 = MyMath::Subtract(t.p[2], t.p[0]);
            t.normal = MyMath::Normalize(MyMath::Cross(edge1, edge2));
            alignNormalToVertexNormals(t, v0, v1, v2);
            worldTriangles_.push_back(t);
        }
    } else {
        for (size_t i = 0; i < modelData.vertices.size(); i += 3) {
            if (i + 2 >= modelData.vertices.size()) break;
            const VertexData& vertex0 = modelData.vertices[i];
            const VertexData& vertex1 = modelData.vertices[i + 1];
            const VertexData& vertex2 = modelData.vertices[i + 2];
            Triangle t;
            t.p[0] = getTransformedPos(vertex0.position);
            t.p[1] = getTransformedPos(vertex1.position);
            t.p[2] = getTransformedPos(vertex2.position);
            Vector3 edge1 = MyMath::Subtract(t.p[1], t.p[0]);
            Vector3 edge2 = MyMath::Subtract(t.p[2], t.p[0]);
            t.normal = MyMath::Normalize(MyMath::Cross(edge1, edge2));
            alignNormalToVertexNormals(t, vertex0, vertex1, vertex2);
            worldTriangles_.push_back(t);
        }
    }
    
    RebuildMeshTriangleGrid();
    prevPosition_ = position_;
    prevRotation_ = rotation_;
    prevScale_ = scale_;
}

void Obstacle::RebuildMeshTriangleGrid() {
    for (auto& cell : meshTriangleGrid_) {
        cell.clear();
    }
    allTriangleIndices_.resize(worldTriangles_.size());
    std::iota(allTriangleIndices_.begin(), allTriangleIndices_.end(), size_t{ 0 });
    nearbyTriangleIndices_.clear();
    hasMeshTriangleGrid_ = false;
    if (worldTriangles_.empty()) {
        return;
    }

    meshGridMin_ = worldTriangles_[0].p[0];
    meshGridMax_ = worldTriangles_[0].p[0];
    for (const Triangle& triangle : worldTriangles_) {
        for (const Vector3& point : triangle.p) {
            meshGridMin_.x = (std::min)(meshGridMin_.x, point.x);
            meshGridMin_.z = (std::min)(meshGridMin_.z, point.z);
            meshGridMax_.x = (std::max)(meshGridMax_.x, point.x);
            meshGridMax_.z = (std::max)(meshGridMax_.z, point.z);
        }
    }

    const float extentX = meshGridMax_.x - meshGridMin_.x;
    const float extentZ = meshGridMax_.z - meshGridMin_.z;
    if (extentX <= 0.0001f || extentZ <= 0.0001f) {
        return;
    }

    const auto toCell = [](float coordinate, float minimum, float extent) {
        const float normalized = (coordinate - minimum) / extent;
        return std::clamp(static_cast<int>(std::floor(normalized * kMeshGridDimension)), 0, kMeshGridDimension - 1);
    };
    for (size_t index = 0; index < worldTriangles_.size(); ++index) {
        const Triangle& triangle = worldTriangles_[index];
        float minX = triangle.p[0].x;
        float maxX = triangle.p[0].x;
        float minZ = triangle.p[0].z;
        float maxZ = triangle.p[0].z;
        for (int pointIndex = 1; pointIndex < 3; ++pointIndex) {
            minX = (std::min)(minX, triangle.p[pointIndex].x);
            maxX = (std::max)(maxX, triangle.p[pointIndex].x);
            minZ = (std::min)(minZ, triangle.p[pointIndex].z);
            maxZ = (std::max)(maxZ, triangle.p[pointIndex].z);
        }
        const int minCellX = toCell(minX, meshGridMin_.x, extentX);
        const int maxCellX = toCell(maxX, meshGridMin_.x, extentX);
        const int minCellZ = toCell(minZ, meshGridMin_.z, extentZ);
        const int maxCellZ = toCell(maxZ, meshGridMin_.z, extentZ);
        for (int cellZ = minCellZ; cellZ <= maxCellZ; ++cellZ) {
            for (int cellX = minCellX; cellX <= maxCellX; ++cellX) {
                meshTriangleGrid_[static_cast<size_t>(cellZ * kMeshGridDimension + cellX)].push_back(index);
            }
        }
    }
    hasMeshTriangleGrid_ = true;
}
