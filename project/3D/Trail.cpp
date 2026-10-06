#include "Trail.h"
#include <algorithm>
#include <cmath>

void Trail::Initialize(int maxPoints) {
	maxPoints_ = maxPoints;
	points_.clear();
}

void Trail::Update(const Vector3 &currentPos) {
    // 最新の座標を先頭に追加
    points_.push_front(currentPos);

    // 履歴が上限を超えたら、一番古い尻尾の座標を消す
    if (points_.size() > maxPoints_) {
        points_.pop_back();
    }
}

void Trail::SetOrigin(const Vector3& localOrigin) {
    points_.clear();
    points_.push_front(localOrigin);
}

std::vector<VertexData> Trail::GenerateVertices(Camera *camera, float thickness) {
    std::vector<VertexData> vertices;

    // 点が2つ未満なら線を引けないので空を返す
    if (points_.size() < 2) return vertices;

    // カメラの現在座標を取得
    Vector3 camPos = camera->GetTranslate();

    // 各ポイントに対して、左右の頂点を2つずつ作る
    for (size_t i = 0; i < points_.size(); ++i) {
        Vector3 dir = { 0, 0, 0 };

        // 1. 軌跡の「進行方向（dir）」を計算する
        if (i < points_.size() - 1) {
            // 次の点へのベクトル
            dir = { points_[i].x - points_[i + 1].x, points_[i].y - points_[i + 1].y, points_[i].z - points_[i + 1].z };
        } else {
            // 最後の点は1つ前の方向をそのまま使う
            dir = { points_[i - 1].x - points_[i].x, points_[i - 1].y - points_[i].y, points_[i - 1].z - points_[i].z };
        }
        dir = MyMath::Normalize(dir);

        // 2. その点から「カメラに向かう方向（toCam）」を計算する
        Vector3 toCam = { camPos.x - points_[i].x, camPos.y - points_[i].y, camPos.z - points_[i].z };
        toCam = MyMath::Normalize(toCam);

        // 3. 外積（Cross）を使って、進行方向とカメラ方向の両方に垂直な「左右の軸（right）」を作る！
        // これにより、どんなにぐねぐね曲がっても常にカメラの方を向くリボンになります。
        Vector3 right = MyMath::Cross(dir, toCam);
        right = MyMath::Normalize(right);

        // 4. リアルな煙の表現：先端（progress=1.0）は細く、尻尾（progress=0.0）にいくほど太く広がる
        float progress = 1.0f - ((float)i / (points_.size() - 1));
        
        // 指数関数的に広がる（最初は細く、あとで大きく広がる）
        float spread = std::pow(1.0f - progress, 1.2f);
        float currentThickness = thickness * (0.3f + spread * 3.0f);

        // 揺らぎ（ノイズ）を追加して、煙がもくもくしている感を出す
        // 頂点インデックス(i)を使って疑似ランダムな揺れ幅を作る
        float noiseX = (std::sin(i * 1.5f) * 0.2f) * spread;
        float noiseY = (std::cos(i * 1.3f) * 0.2f) * spread;
        float noiseZ = (std::sin(i * 1.1f) * 0.2f) * spread;

        // 左右の頂点座標（ノイズを足す）
        Vector3 leftPos = {
            points_[i].x - right.x * currentThickness + noiseX,
            points_[i].y - right.y * currentThickness + noiseY,
            points_[i].z - right.z * currentThickness + noiseZ
        };
        Vector3 rightPos = {
            points_[i].x + right.x * currentThickness + noiseX,
            points_[i].y + right.y * currentThickness + noiseY,
            points_[i].z + right.z * currentThickness + noiseZ
        };

        // 5. 頂点データの生成
        VertexData vLeft, vRight;

        // 煙の色：先端はオレンジ（炎）、後ろはグレー（煙）
        float r = 0.3f + progress * 0.7f;
        float g = 0.3f + progress * 0.4f;
        float b = 0.3f + progress * 0.1f;
        
        // 尻尾にかけてフェードアウト（アルファ値）
        float alpha = std::pow(progress, 1.5f);

        // 左側の頂点
        vLeft.position = { leftPos.x, leftPos.y, leftPos.z, 1.0f };
        vLeft.normal = { toCam.x, toCam.y, toCam.z, 0.0f }; // 法線はカメラの方向
        vLeft.texcoord = { 0.0f, 1.0f - progress, 0.0f, 0.0f }; // V方向にスクロールするようにUVを設定
        vLeft.color = { r, g, b, alpha };

        // 右側の頂点
        vRight.position = { rightPos.x, rightPos.y, rightPos.z, 1.0f };
        vRight.normal = { toCam.x, toCam.y, toCam.z, 0.0f };
        vRight.texcoord = { 1.0f, 1.0f - progress, 0.0f, 0.0f };
        vRight.color = { r, g, b, alpha };

        // リストに追加（TriangleStripで描画される前提の順番）
        vertices.push_back(vLeft);
        vertices.push_back(vRight);
    }

    return vertices;
}

std::vector<VertexData> Trail::GenerateThrusterVertices(Camera* camera, float thickness,
    const Vector3& exhaustDirection, float plumeLength, float phase, float intensity,
    float coreLengthRatio, float glowScale) {
    (void)camera; // All layers have a circular 3D cross-section; no billboard axes.
    std::vector<VertexData> vertices;
    if (points_.empty() || thickness <= 0.0f || plumeLength <= 0.0f) return vertices;

    constexpr int kSegments = 32;
    constexpr int kSides = 16;
    constexpr float kPi = 3.14159265359f;
    const auto safeNormalize = [](Vector3 v) {
        const float squared = v.x * v.x + v.y * v.y + v.z * v.z;
        if (squared < 1.0e-8f) return Vector3{ 0.0f, 0.0f, -1.0f };
        const float inverse = 1.0f / std::sqrt(squared);
        return Vector3{ v.x * inverse, v.y * inverse, v.z * inverse };
    };
    const auto smoothFade = [](float value) {
        const float x = std::clamp(value, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    };
    const Vector3 axis = safeNormalize(exhaustDirection);
    const Vector3 origin = points_.front();
    const Vector3 reference = std::abs(axis.y) < 0.9f
        ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
    const Vector3 normal = safeNormalize(MyMath::Cross(axis, reference));
    const Vector3 binormal = safeNormalize(MyMath::Cross(axis, normal));

    struct Layer {
        float radiusScale;
        float lengthScale;
        Vector4 emission;
    };
    // A white-hot inner volume, pink outer flame, and a faint violet halo.
    // Each layer tapers to a point: rear views show volume, never a flat end cap.
    const Layer layers[] = {
        { 1.90f, 1.04f, { 0.78f, 0.03f, 1.0f, 0.20f * std::clamp(glowScale, 0.0f, 3.0f) } },
        { 1.00f, 1.00f, { 1.00f, 0.04f, 0.55f, 0.72f } },
        { 0.46f, std::clamp(coreLengthRatio, 0.2f, 0.8f), { 1.05f, 0.82f, 1.00f, 0.90f } }
    };
    vertices.reserve(kSegments * kSides * 6 * 3);
    intensity = std::clamp(intensity, 0.0f, 1.0f);

    for (const Layer& layer : layers) {
        // Keep width and length independent when the editor makes a broad, short flame.
        const float length = (std::max)(plumeLength * layer.lengthScale, 0.01f);
        const auto radiusAt = [&](float t) {
            const float profile = 1.45f * std::pow((std::max)(0.0f, std::sin(kPi * t)), 0.35f)
                * std::pow(1.0f - t, 0.65f);
            const float flicker = 1.0f + 0.025f * std::sin(phase + t * 19.0f);
            return thickness * layer.radiusScale * profile * flicker;
        };
        const auto makeVertex = [&](float t, int side) {
            const float angle = 2.0f * kPi * static_cast<float>(side) / kSides;
            const Vector3 radial = {
                normal.x * std::cos(angle) + binormal.x * std::sin(angle),
                normal.y * std::cos(angle) + binormal.y * std::sin(angle),
                normal.z * std::cos(angle) + binormal.z * std::sin(angle)
            };
            const float radius = radiusAt(t);
            const float waviness = std::sin(phase * 0.8f + t * 12.0f)
                * thickness * 0.04f * std::sin(kPi * t);
            const Vector3 center = {
                origin.x + axis.x * length * t + normal.x * waviness,
                origin.y + axis.y * length * t + normal.y * waviness,
                origin.z + axis.z * length * t + normal.z * waviness
            };
            // Include the taper slope in the normal, so the rounded rear-facing
            // surface remains lit by the emissive shader when viewed down the axis.
            const float tBefore = (std::max)(0.0f, t - 0.005f);
            const float tAfter = (std::min)(1.0f, t + 0.005f);
            const float slope = (radiusAt(tAfter) - radiusAt(tBefore)) / (length * (tAfter - tBefore));
            const Vector3 surfaceNormal = safeNormalize({
                radial.x - axis.x * slope, radial.y - axis.y * slope, radial.z - axis.z * slope
            });
            VertexData v{};
            v.position = { center.x + radial.x * radius, center.y + radial.y * radius,
                center.z + radial.z * radius, 1.0f };
            v.normal = { surfaceNormal.x, surfaceNormal.y, surfaceNormal.z, 0.0f };
            v.texcoord = { static_cast<float>(side) / kSides, t, 0.0f, 0.0f };
            const float fade = smoothFade(t / 0.035f) * smoothFade((1.0f - t) / 0.38f)
                * std::pow(1.0f - t, 0.80f);
            v.color = { layer.emission.x, layer.emission.y, layer.emission.z,
                layer.emission.w * intensity * fade };
            return v;
        };
        for (int segment = 0; segment < kSegments; ++segment) {
            const float t0 = static_cast<float>(segment) / kSegments;
            const float t1 = static_cast<float>(segment + 1) / kSegments;
            for (int side = 0; side < kSides; ++side) {
                const VertexData a = makeVertex(t0, side);
                const VertexData b = makeVertex(t0, side + 1);
                const VertexData c = makeVertex(t1, side);
                const VertexData d = makeVertex(t1, side + 1);
                vertices.insert(vertices.end(), { a, b, c, b, d, c });
            }
        }
    }
    return vertices;
}
