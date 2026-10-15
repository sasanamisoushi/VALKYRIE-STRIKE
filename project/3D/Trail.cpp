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

std::vector<VertexData> Trail::GenerateRibbonVertices(Camera* camera, float thickness,
    const Vector4& headColor, const Vector3& nozzleExhaustDirection,
    const Vector3& nozzlePosition, const Vector3& plumeTipPosition,
    float intensity, float maxLength, float coreFadeLength) {
    std::vector<VertexData> vertices;
    if (!camera || points_.empty() || thickness <= 0.0f || intensity <= 0.001f) {
        return vertices;
    }

    const Vector3 cameraPosition = camera->GetTranslate();
    const auto lengthSquared = [](const Vector3& value) {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    };
    const auto normalizeOr = [&](const Vector3& value, const Vector3& fallback) {
        const float squared = lengthSquared(value);
        if (squared <= 0.000001f) {
            return fallback;
        }
        const float inverseLength = 1.0f / std::sqrt(squared);
        return Vector3{ value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
    };

    // 炎と光跡を一本にする。ノズル口から炎の先端へ進み、そこから遅延した
    // 移動履歴へ連続させるため、すべてを一つのリボンの制御点にする。
    std::vector<Vector3> sourcePoints;
    sourcePoints.reserve(points_.size() + 2);
    sourcePoints.push_back(nozzlePosition);
    if (lengthSquared({
            plumeTipPosition.x - sourcePoints.back().x,
            plumeTipPosition.y - sourcePoints.back().y,
            plumeTipPosition.z - sourcePoints.back().z }) > 0.00000001f) {
        sourcePoints.push_back(plumeTipPosition);
    }
    for (const Vector3& point : points_) {
        if (lengthSquared({
                point.x - sourcePoints.back().x,
                point.y - sourcePoints.back().y,
                point.z - sourcePoints.back().z }) > 0.00000001f) {
            sourcePoints.push_back(point);
        }
    }
    if (sourcePoints.size() < 2) return vertices;

    // The history is sampled once per frame, so simply joining the points makes
    // a hard V-shaped corner when the player turns sharply.  Resample a cubic
    // Catmull-Rom spline first; every original history point remains a control
    // point, but the rendered ribbon rounds the transition between them.
    const auto catmullRom = [](const Vector3& p0, const Vector3& p1,
        const Vector3& p2, const Vector3& p3, float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return Vector3{
            0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
            0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3),
            0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t +
                (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 +
                (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3)
        };
    };
    const auto cubicHermite = [](const Vector3& start, const Vector3& end,
        const Vector3& startTangent, const Vector3& endTangent, float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        const float h10 = t3 - 2.0f * t2 + t;
        const float h01 = -2.0f * t3 + 3.0f * t2;
        const float h11 = t3 - t2;
        return Vector3{
            h00 * start.x + h10 * startTangent.x + h01 * end.x + h11 * endTangent.x,
            h00 * start.y + h10 * startTangent.y + h01 * end.y + h11 * endTangent.y,
            h00 * start.z + h10 * startTangent.z + h01 * end.z + h11 * endTangent.z
        };
    };
    std::vector<Vector3> curvePoints;
    curvePoints.reserve(sourcePoints.size() * 4);
    const auto appendPoint = [&](const Vector3& point) {
        if (curvePoints.empty() || lengthSquared({
                point.x - curvePoints.back().x,
                point.y - curvePoints.back().y,
                point.z - curvePoints.back().z }) > 0.00000001f) {
            curvePoints.push_back(point);
        }
    };
    for (size_t segment = 0; segment + 1 < sourcePoints.size(); ++segment) {
        const Vector3& p0 = sourcePoints[segment == 0 ? 0 : segment - 1];
        const Vector3& p1 = sourcePoints[segment];
        const Vector3& p2 = sourcePoints[segment + 1];
        const Vector3& p3 = sourcePoints[(std::min)(segment + 2, sourcePoints.size() - 1)];
        const Vector3 segmentDelta = { p2.x - p1.x, p2.y - p1.y, p2.z - p1.z };
        const float segmentLength = std::sqrt(lengthSquared(segmentDelta));
        // Keep the smooth curve dense enough even at low frame rates, without
        // generating an unbounded number of vertices during a teleport/jump.
        const int subdivisions = std::clamp(
            static_cast<int>(std::ceil(segmentLength / (std::max)(thickness * 0.65f, 0.025f))), 2, 12);
        for (int step = 0; step < subdivisions; ++step) {
            const float t = static_cast<float>(step) / subdivisions;
            if (segment == 0 && segmentLength > 0.000001f && lengthSquared(nozzleExhaustDirection) > 0.000001f) {
                // ノズルの向きと直前フレームの移動方向が大きく異なるときに、
                // 排気方向をそのまま接線にすると L 字の折れ曲がりになる。
                // その場合は履歴の接線へ滑らかに寄せ、方向がほぼ一致している
                // ときだけノズル向きを強く残す。
                const Vector3 historyDirection = normalizeOr(segmentDelta, { 0.0f, 0.0f, -1.0f });
                const Vector3 exhaustDirection = normalizeOr(nozzleExhaustDirection, historyDirection);
                const float directionAlignment = std::clamp(
                    exhaustDirection.x * historyDirection.x +
                    exhaustDirection.y * historyDirection.y +
                    exhaustDirection.z * historyDirection.z, -1.0f, 1.0f);
                // 90 度以上の差では排気方向の影響をほぼ除き、45 度以内で
                // 徐々に本来のノズル方向へ戻す。これで急旋回時も丸い弧になる。
                const float nozzleInfluence = std::clamp((directionAlignment - 0.05f) / 0.80f, 0.0f, 1.0f);
                const Vector3 direction = normalizeOr({
                    historyDirection.x + (exhaustDirection.x - historyDirection.x) * nozzleInfluence,
                    historyDirection.y + (exhaustDirection.y - historyDirection.y) * nozzleInfluence,
                    historyDirection.z + (exhaustDirection.z - historyDirection.z) * nozzleInfluence
                }, historyDirection);
                const Vector3 startTangent = {
                    // 接線を短くして、最初の 1 区間だけが直線に見えるのを防ぐ。
                    direction.x * segmentLength * (0.20f + 0.20f * nozzleInfluence),
                    direction.y * segmentLength * (0.20f + 0.20f * nozzleInfluence),
                    direction.z * segmentLength * (0.20f + 0.20f * nozzleInfluence)
                };
                const Vector3 endTangent = {
                    (p3.x - p1.x) * 0.5f,
                    (p3.y - p1.y) * 0.5f,
                    (p3.z - p1.z) * 0.5f
                };
                appendPoint(cubicHermite(p1, p2, startTangent, endTangent, t));
            } else {
                appendPoint(catmullRom(p0, p1, p2, p3, t));
            }
        }
    }
    appendPoint(sourcePoints.back());
    if (curvePoints.size() < 2) return vertices;

    // A high-speed frame can move farther than several normal frames.  Crop
    // by world distance after the curve is generated, rather than by a fixed
    // number of samples, so the trail cannot suddenly stretch off-screen.
    if (maxLength > 0.0f) {
        std::vector<Vector3> limitedPoints;
        limitedPoints.reserve(curvePoints.size());
        limitedPoints.push_back(curvePoints.front());
        float remainingLength = maxLength;
        for (size_t index = 1; index < curvePoints.size() && remainingLength > 0.0f; ++index) {
            const Vector3& previous = limitedPoints.back();
            const Vector3& candidate = curvePoints[index];
            const Vector3 delta = {
                candidate.x - previous.x,
                candidate.y - previous.y,
                candidate.z - previous.z
            };
            const float segmentLength = std::sqrt(lengthSquared(delta));
            if (segmentLength <= remainingLength) {
                limitedPoints.push_back(candidate);
                remainingLength -= segmentLength;
            } else if (segmentLength > 0.000001f) {
                const float ratio = remainingLength / segmentLength;
                limitedPoints.push_back({
                    previous.x + delta.x * ratio,
                    previous.y + delta.y * ratio,
                    previous.z + delta.z * ratio
                });
                break;
            }
        }
        curvePoints = std::move(limitedPoints);
    }
    if (curvePoints.size() < 2) return vertices;

    std::vector<float> distances(curvePoints.size(), 0.0f);
    for (size_t index = 1; index < curvePoints.size(); ++index) {
        distances[index] = distances[index - 1] + std::sqrt(lengthSquared({
            curvePoints[index].x - curvePoints[index - 1].x,
            curvePoints[index].y - curvePoints[index - 1].y,
            curvePoints[index].z - curvePoints[index - 1].z }));
    }
    const float totalDistance = distances.back();
    if (totalDistance <= 0.000001f) return vertices;

    vertices.reserve(curvePoints.size() * 2);
    for (size_t index = 0; index < curvePoints.size(); ++index) {
        Vector3 direction{};
        if (index + 1 < curvePoints.size()) {
            direction = {
                curvePoints[index].x - curvePoints[index + 1].x,
                curvePoints[index].y - curvePoints[index + 1].y,
                curvePoints[index].z - curvePoints[index + 1].z
            };
        }
        if (lengthSquared(direction) <= 0.000001f && index > 0) {
            direction = {
                curvePoints[index - 1].x - curvePoints[index].x,
                curvePoints[index - 1].y - curvePoints[index].y,
                curvePoints[index - 1].z - curvePoints[index].z
            };
        }
        direction = normalizeOr(direction, { 0.0f, 0.0f, 1.0f });

        const Vector3 toCamera = normalizeOr({
            cameraPosition.x - curvePoints[index].x,
            cameraPosition.y - curvePoints[index].y,
            cameraPosition.z - curvePoints[index].z
        }, { 0.0f, 1.0f, 0.0f });
        Vector3 right = MyMath::Cross(direction, toCamera);
        if (lengthSquared(right) <= 0.000001f) {
            right = MyMath::Cross(direction, { 0.0f, 1.0f, 0.0f });
        }
        if (lengthSquared(right) <= 0.000001f) {
            right = MyMath::Cross(direction, { 1.0f, 0.0f, 0.0f });
        }
        right = normalizeOr(right, { 1.0f, 0.0f, 0.0f });

        // Distance-based lifetime makes the fade stable even when a fast frame
        // inserts more spline samples than a slow one.
        const float progress = 1.0f - distances[index] / totalDistance;
        // ノズル側から末端まで連続的に細くし、最後の一点だけで切れないようにする。
        // 横方向のぼかしは RibbonTrail.PS.hlsl が担当する。
        const float halfWidth = thickness * std::pow(progress, 0.72f);
        const float fade = progress * progress * (3.0f - 2.0f * progress);
        const float colorScale = 0.20f + 0.80f * std::sqrt(progress);
        // 立体炎と重なる最初の区間も消さず、低い強度からなめらかに重ねる。
        // これが炎の末端と光跡を同一の発光体に見せる接続部になる。急旋回時の
        // 面の折れは弱く抑えつつ、透明な隙間は作らない。
        const float coreBlend = coreFadeLength > 0.0001f
            ? std::clamp(distances[index] / ((std::max)(coreFadeLength * 0.92f, 0.0001f)), 0.0f, 1.0f)
            : 1.0f;
        const float smoothCoreBlend = coreBlend * coreBlend * (3.0f - 2.0f * coreBlend);
        const float coreOpacity = 0.18f + 0.82f * smoothCoreBlend;
        const Vector4 color = {
            headColor.x * colorScale,
            headColor.y * colorScale,
            headColor.z * colorScale,
            headColor.w * intensity * fade * coreOpacity
        };

        VertexData left{};
        left.position = {
            curvePoints[index].x - right.x * halfWidth,
            curvePoints[index].y - right.y * halfWidth,
            curvePoints[index].z - right.z * halfWidth,
            1.0f
        };
        left.normal = { toCamera.x, toCamera.y, toCamera.z, 0.0f };
        left.texcoord = { 0.0f, 1.0f - progress, 0.0f, 0.0f };
        left.color = color;

        VertexData rightVertex = left;
        rightVertex.position = {
            curvePoints[index].x + right.x * halfWidth,
            curvePoints[index].y + right.y * halfWidth,
            curvePoints[index].z + right.z * halfWidth,
            1.0f
        };
        rightVertex.texcoord.x = 1.0f;
        vertices.push_back(left);
        vertices.push_back(rightVertex);
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
