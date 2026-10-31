#include "Trail.h"
#include <algorithm>
#include <cmath>

void Trail::Initialize(int maxPoints) {
	maxPoints_ = maxPoints;
	points_.clear();
	lifeTime_ = 0.55f;
}

void Trail::Update(const Vector3& currentPos, float deltaTime, float lifeTime) {
    lifeTime_ = (std::max)(lifeTime, 0.01f);
    const float safeDeltaTime = (std::max)(deltaTime, 0.0f);
    for (TrailPoint& point : points_) {
        point.age += safeDeltaTime;
    }
    while (!points_.empty() && points_.back().age > lifeTime_) {
        points_.pop_back();
    }

    const auto lengthSquared = [](const Vector3& value) {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    };
    if (!hasHeadPosition_) {
        headPosition_ = currentPos;
        hasHeadPosition_ = true;
        distanceSinceLastSample_ = 0.0f;
        points_.push_front({ currentPos, 0.0f });
        return;
    }

    const Vector3 previousHead = headPosition_;
    const Vector3 delta = { currentPos.x - previousHead.x,
        currentPos.y - previousHead.y, currentPos.z - previousHead.z };
    const float distanceSquared = lengthSquared(delta);
    if (distanceSquared > 1.0e-8f) {
        const float distance = std::sqrt(distanceSquared);
        const Vector3 direction = { delta.x / distance, delta.y / distance, delta.z / distance };
        float travelledThisFrame = 0.0f;
        float remainingDistance = distance;

        // 1フレームに大きく移動しても、前の採取点から一定間隔で補間点を入れる。
        // 追加順は古い→新しいだが push_front するため、deque の先頭は常に最新になる。
        while (distanceSinceLastSample_ + remainingDistance >= kSampleSpacing_) {
            const float distanceToSample = kSampleSpacing_ - distanceSinceLastSample_;
            travelledThisFrame += distanceToSample;
            const float t = std::clamp(travelledThisFrame / distance, 0.0f, 1.0f);
            const Vector3 samplePosition = { previousHead.x + direction.x * travelledThisFrame,
                previousHead.y + direction.y * travelledThisFrame,
                previousHead.z + direction.z * travelledThisFrame };
            // フレーム内補間点にも生成時刻を割り当て、寿命フェードを連続させる。
            points_.push_front({ samplePosition, safeDeltaTime * (1.0f - t) });
            remainingDistance -= distanceToSample;
            distanceSinceLastSample_ = 0.0f;
        }
        distanceSinceLastSample_ += remainingDistance;
    }

    headPosition_ = currentPos;
    while (static_cast<int>(points_.size()) > maxPoints_) {
        points_.pop_back();
    }
}

void Trail::SetOrigin(const Vector3& localOrigin) {
    points_.clear();
    points_.push_front({ localOrigin, 0.0f });
    headPosition_ = localOrigin;
    hasHeadPosition_ = true;
    distanceSinceLastSample_ = 0.0f;
}

std::vector<VertexData> Trail::GenerateVertices(Camera *camera, float thickness) {
    std::vector<VertexData> vertices;
    if (points_.size() < 2) return vertices;
    const Vector3 camPos = camera->GetTranslate();
    for (size_t i = 0; i < points_.size(); ++i) {
        const Vector3& point = points_[i].position;
        Vector3 dir{};
        if (i < points_.size() - 1) {
            const Vector3& next = points_[i + 1].position;
            dir = { point.x - next.x, point.y - next.y, point.z - next.z };
        } else {
            const Vector3& previous = points_[i - 1].position;
            dir = { previous.x - point.x, previous.y - point.y, previous.z - point.z };
        }
        dir = MyMath::Normalize(dir);
        Vector3 toCam = { camPos.x - point.x, camPos.y - point.y, camPos.z - point.z };
        toCam = MyMath::Normalize(toCam);
        Vector3 right = MyMath::Cross(dir, toCam);
        right = MyMath::Normalize(right);
        float progress = 1.0f - ((float)i / (points_.size() - 1));
        float spread = std::pow(1.0f - progress, 1.2f);
        float currentThickness = thickness * (0.3f + spread * 3.0f);
        float noiseX = (std::sin(i * 1.5f) * 0.2f) * spread;
        float noiseY = (std::cos(i * 1.3f) * 0.2f) * spread;
        float noiseZ = (std::sin(i * 1.1f) * 0.2f) * spread;
        Vector3 leftPos = {
            point.x - right.x * currentThickness + noiseX,
            point.y - right.y * currentThickness + noiseY,
            point.z - right.z * currentThickness + noiseZ
        };
        Vector3 rightPos = {
            point.x + right.x * currentThickness + noiseX,
            point.y + right.y * currentThickness + noiseY,
            point.z + right.z * currentThickness + noiseZ
        };
        VertexData vLeft, vRight;
        float r = 0.3f + progress * 0.7f;
        float g = 0.3f + progress * 0.4f;
        float b = 0.3f + progress * 0.1f;
        float alpha = std::pow(progress, 1.5f);
        vLeft.position = { leftPos.x, leftPos.y, leftPos.z, 1.0f };
        vLeft.normal = { toCam.x, toCam.y, toCam.z, 0.0f };
        vLeft.texcoord = { 0.0f, 1.0f - progress, 0.0f, 0.0f };
        vLeft.color = { r, g, b, alpha };
        vRight.position = { rightPos.x, rightPos.y, rightPos.z, 1.0f };
        vRight.normal = { toCam.x, toCam.y, toCam.z, 0.0f };
        vRight.texcoord = { 1.0f, 1.0f - progress, 0.0f, 0.0f };
        vRight.color = { r, g, b, alpha };
        vertices.push_back(vLeft);
        vertices.push_back(vRight);
    }

    return vertices;
}

// 前世代の距離制限型帯。現在は寿命付き履歴リボンへ置き換えたため参照しない。
#if 0
std::vector<VertexData> Trail::GenerateSpeedTrailVertices(Camera* camera, float thickness,
    const Vector3& fixedFlameStart, const Vector3& exhaustDirection,
    float intensity, float maximumLength) {
    std::vector<VertexData> vertices;
    if (!camera || points_.size() < 2 || thickness <= 0.0f || intensity <= 0.01f) return vertices;

    const auto lengthSquared = [](const Vector3& value) {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    };
    const auto normalizeOr = [&](const Vector3& value, const Vector3& fallback) {
        const float squared = lengthSquared(value);
        if (squared <= 1.0e-8f) return fallback;
        const float inverseLength = 1.0f / std::sqrt(squared);
        return Vector3{ value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
    };
    const auto appendPoint = [&](std::vector<Vector3>& destination, const Vector3& point) {
        if (destination.empty() || lengthSquared({ point.x - destination.back().x,
                point.y - destination.back().y, point.z - destination.back().z }) > 1.0e-8f) {
            destination.push_back(point);
        }
    };
    // 均一パラメータの Catmull-Rom は点間距離が偏ると曲線が外へ膨らむため、
    // 弦長の平方根で時刻を決める centripetal Catmull-Rom を使う。
    // 急旋回してもループや直下への飛び出しを作りにくい。
    const auto catmullRom = [&](const Vector3& p0, const Vector3& p1,
        const Vector3& p2, const Vector3& p3, float t) {
        const auto parameterStep = [&](const Vector3& from, const Vector3& to) {
            return (std::max)(std::sqrt(std::sqrt(lengthSquared({ to.x - from.x,
                to.y - from.y, to.z - from.z }))), 1.0e-4f);
        };
        const auto interpolate = [](const Vector3& from, const Vector3& to,
            float fromTime, float toTime, float time) {
            const float denominator = toTime - fromTime;
            if (std::abs(denominator) <= 1.0e-6f) return from;
            const float ratio = std::clamp((time - fromTime) / denominator, 0.0f, 1.0f);
            return Vector3{ from.x + (to.x - from.x) * ratio,
                from.y + (to.y - from.y) * ratio,
                from.z + (to.z - from.z) * ratio };
        };
        const float t0 = 0.0f;
        const float t1 = t0 + parameterStep(p0, p1);
        const float t2 = t1 + parameterStep(p1, p2);
        const float t3 = t2 + parameterStep(p2, p3);
        const float curveTime = t1 + (t2 - t1) * t;
        const Vector3 a1 = interpolate(p0, p1, t0, t1, curveTime);
        const Vector3 a2 = interpolate(p1, p2, t1, t2, curveTime);
        const Vector3 a3 = interpolate(p2, p3, t2, t3, curveTime);
        const Vector3 b1 = interpolate(a1, a2, t0, t2, curveTime);
        const Vector3 b2 = interpolate(a2, a3, t1, t3, curveTime);
        return interpolate(b1, b2, t1, t2, curveTime);
    };

    const Vector3 axis = normalizeOr(exhaustDirection, { 0.0f, 0.0f, -1.0f });
    // 根元は固定炎の内側から始める。最初の履歴点までをいきなり移動方向へ
    // 曲げないので、旋回直後にもノズル付近へ境目や直角を作らない。
    std::vector<Vector3> controls;
    controls.reserve(points_.size() + 1);
    controls.push_back(fixedFlameStart);
    float traveled = 0.0f;
    Vector3 previous = fixedFlameStart;
    for (const Vector3& point : points_) {
        const Vector3 delta = { point.x - previous.x, point.y - previous.y, point.z - previous.z };
        const float segmentLength = std::sqrt(lengthSquared(delta));
        if (maximumLength > 0.0f && traveled + segmentLength > maximumLength) {
            if (segmentLength > 1.0e-6f) {
                const float ratio = (maximumLength - traveled) / segmentLength;
                appendPoint(controls, { previous.x + delta.x * ratio,
                    previous.y + delta.y * ratio, previous.z + delta.z * ratio });
            }
            break;
        }
        appendPoint(controls, point);
        traveled += segmentLength;
        previous = point;
    }
    if (controls.size() < 3 || traveled <= thickness * 0.20f) return vertices;

    std::vector<Vector3> curvePoints;
    curvePoints.reserve(controls.size() * 3);
    curvePoints.push_back(controls.front());
    constexpr int kSubdivisions = 3;
    for (size_t index = 0; index + 1 < controls.size(); ++index) {
        const Vector3& p0 = controls[index == 0 ? 0 : index - 1];
        const Vector3& p1 = controls[index];
        const Vector3& p2 = controls[index + 1];
        const Vector3& p3 = controls[(std::min)(index + 2, controls.size() - 1)];
        for (int step = 1; step <= kSubdivisions; ++step) {
            appendPoint(curvePoints, catmullRom(p0, p1, p2, p3,
                static_cast<float>(step) / kSubdivisions));
        }
    }
    // リボンと立体炎の経路を完全に一致させる。上の旧構築部は互換性を保つ
    // ガードだが、実描画には共通の centripetal 曲線だけを使う。
    curvePoints = BuildSpeedTrailCurve(fixedFlameStart, exhaustDirection, safeLifeTime);
    if (curvePoints.size() < 2) return vertices;

    std::vector<float> distances(curvePoints.size(), 0.0f);
    for (size_t index = 1; index < curvePoints.size(); ++index) {
        distances[index] = distances[index - 1] + std::sqrt(lengthSquared({
            curvePoints[index].x - curvePoints[index - 1].x,
            curvePoints[index].y - curvePoints[index - 1].y,
            curvePoints[index].z - curvePoints[index - 1].z }));
    }
    const float totalDistance = distances.back();
    if (totalDistance <= 1.0e-6f) return vertices;

    vertices.reserve(curvePoints.size() * 2);
    const Vector3 cameraPosition = camera->GetTranslate();
    for (size_t index = 0; index < curvePoints.size(); ++index) {
        Vector3 tangent = index == 0
            ? Vector3{ curvePoints[1].x - curvePoints[0].x, curvePoints[1].y - curvePoints[0].y, curvePoints[1].z - curvePoints[0].z }
            : (index + 1 == curvePoints.size()
                ? Vector3{ curvePoints[index].x - curvePoints[index - 1].x, curvePoints[index].y - curvePoints[index - 1].y, curvePoints[index].z - curvePoints[index - 1].z }
                : Vector3{ curvePoints[index + 1].x - curvePoints[index - 1].x, curvePoints[index + 1].y - curvePoints[index - 1].y, curvePoints[index + 1].z - curvePoints[index - 1].z });
        tangent = normalizeOr(tangent, axis);
        const Vector3 toCamera = normalizeOr({ cameraPosition.x - curvePoints[index].x,
            cameraPosition.y - curvePoints[index].y, cameraPosition.z - curvePoints[index].z }, { 0.0f, 1.0f, 0.0f });
        Vector3 side = MyMath::Cross(tangent, toCamera);
        if (lengthSquared(side) <= 1.0e-6f) side = MyMath::Cross(tangent, { 0.0f, 1.0f, 0.0f });
        side = normalizeOr(side, { 1.0f, 0.0f, 0.0f });

        const float progress = distances[index] / totalDistance;
        // 根元の幅を固定炎の外周より少し広く保ち、先端だけを柔らかく縮める。
        const float halfWidth = thickness * (0.35f + 1.05f * std::pow(1.0f - progress, 0.55f));
        const float alpha = intensity * (1.0f - progress * 0.30f);
        VertexData left{};
        left.position = { curvePoints[index].x - side.x * halfWidth,
            curvePoints[index].y - side.y * halfWidth, curvePoints[index].z - side.z * halfWidth, 1.0f };
        left.normal = { toCamera.x, toCamera.y, toCamera.z, 0.0f };
        left.texcoord = { 0.0f, progress, 0.0f, 0.0f };
        left.color = { 1.0f, 0.05f, 0.72f, alpha };
        VertexData right = left;
        right.position = { curvePoints[index].x + side.x * halfWidth,
            curvePoints[index].y + side.y * halfWidth, curvePoints[index].z + side.z * halfWidth, 1.0f };
        right.texcoord.x = 1.0f;
        vertices.push_back(left);
        vertices.push_back(right);
    }
    return vertices;
}

#endif

std::vector<Trail::TrailPoint> Trail::BuildSpeedTrailCurve(const Vector3& nozzlePosition,
    const Vector3& exhaustDirection, float lifeTime) const {
    std::vector<TrailPoint> curvePoints;
    if (!hasHeadPosition_) return curvePoints;

    const auto lengthSquared = [](const Vector3& value) {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    };
    const auto normalizeOr = [&](const Vector3& value, const Vector3& fallback) {
        const float squared = lengthSquared(value);
        if (squared <= 1.0e-8f) return fallback;
        const float inverseLength = 1.0f / std::sqrt(squared);
        return Vector3{ value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
    };
    const auto appendPoint = [&](std::vector<TrailPoint>& destination, const TrailPoint& point) {
        if (destination.empty() || lengthSquared({ point.position.x - destination.back().position.x,
                point.position.y - destination.back().position.y,
                point.position.z - destination.back().position.z }) > 1.0e-8f) {
            destination.push_back(point);
        }
    };
    // 点間距離の偏りで曲線が外側へ膨らまない centripetal Catmull-Rom。
    const auto catmullRom = [&](const Vector3& p0, const Vector3& p1,
        const Vector3& p2, const Vector3& p3, float t) {
        const auto parameterStep = [&](const Vector3& from, const Vector3& to) {
            return (std::max)(std::sqrt(std::sqrt(lengthSquared({ to.x - from.x,
                to.y - from.y, to.z - from.z }))), 1.0e-4f);
        };
        const auto interpolate = [](const Vector3& from, const Vector3& to,
            float fromTime, float toTime, float time) {
            const float denominator = toTime - fromTime;
            if (std::abs(denominator) <= 1.0e-6f) return from;
            const float ratio = std::clamp((time - fromTime) / denominator, 0.0f, 1.0f);
            return Vector3{ from.x + (to.x - from.x) * ratio,
                from.y + (to.y - from.y) * ratio,
                from.z + (to.z - from.z) * ratio };
        };
        const float t0 = 0.0f;
        const float t1 = t0 + parameterStep(p0, p1);
        const float t2 = t1 + parameterStep(p1, p2);
        const float t3 = t2 + parameterStep(p2, p3);
        const float curveTime = t1 + (t2 - t1) * t;
        const Vector3 a1 = interpolate(p0, p1, t0, t1, curveTime);
        const Vector3 a2 = interpolate(p1, p2, t1, t2, curveTime);
        const Vector3 a3 = interpolate(p2, p3, t2, t3, curveTime);
        const Vector3 b1 = interpolate(a1, a2, t0, t2, curveTime);
        const Vector3 b2 = interpolate(a2, a3, t1, t3, curveTime);
        return interpolate(b1, b2, t1, t2, curveTime);
    };

    const float safeLifeTime = (std::max)(lifeTime, 0.01f);
    const Vector3 axis = normalizeOr(exhaustDirection, { 0.0f, 0.0f, -1.0f });
    std::vector<TrailPoint> controls;
    controls.reserve(points_.size() + 2);
    controls.push_back({ nozzlePosition, 0.0f });
    appendPoint(controls, { headPosition_, 0.0f });
    for (const TrailPoint& point : points_) {
        if (point.age <= safeLifeTime) appendPoint(controls, point);
    }
    if (controls.size() < 3) return curvePoints;

    curvePoints.reserve(controls.size() * 8);
    curvePoints.push_back(controls.front());
    constexpr int kSubdivisions = 8;
    for (size_t index = 0; index + 1 < controls.size(); ++index) {
        const TrailPoint& p1 = controls[index];
        const TrailPoint& p2 = controls[index + 1];
        const Vector3 segment = { p2.position.x - p1.position.x,
            p2.position.y - p1.position.y, p2.position.z - p1.position.z };
        const float virtualControlDistance = std::clamp(
            std::sqrt(lengthSquared(segment)), 0.06f, 0.22f);
        const Vector3 p0 = index == 0
            ? Vector3{ p1.position.x - axis.x * virtualControlDistance,
                p1.position.y - axis.y * virtualControlDistance,
                p1.position.z - axis.z * virtualControlDistance }
            : controls[index - 1].position;
        const Vector3 p3 = index + 2 >= controls.size()
            ? Vector3{ p2.position.x * 2.0f - p1.position.x,
                p2.position.y * 2.0f - p1.position.y, p2.position.z * 2.0f - p1.position.z }
            : controls[index + 2].position;
        for (int step = 1; step <= kSubdivisions; ++step) {
            const float t = static_cast<float>(step) / kSubdivisions;
            appendPoint(curvePoints, { catmullRom(p0, p1.position, p2.position, p3, t),
                p1.age + (p2.age - p1.age) * t });
        }
    }
    return curvePoints;
}

std::vector<VertexData> Trail::GenerateSpeedTrailVertices(Camera* camera, float thickness,
    const Vector3& fixedFlameStart, const Vector3& exhaustDirection,
    float intensity, float lifeTime, const Vector4& outerColor, const Vector4& coreColor) {
    std::vector<VertexData> vertices;
    if (!camera || points_.size() < 2 || thickness <= 0.0f || intensity <= 0.01f) return vertices;

    const auto lengthSquared = [](const Vector3& value) {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    };
    const auto normalizeOr = [&](const Vector3& value, const Vector3& fallback) {
        const float squared = lengthSquared(value);
        if (squared <= 1.0e-8f) return fallback;
        const float inverseLength = 1.0f / std::sqrt(squared);
        return Vector3{ value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
    };
    const auto appendPoint = [&](std::vector<TrailPoint>& destination, const TrailPoint& point) {
        if (destination.empty() || lengthSquared({ point.position.x - destination.back().position.x,
                point.position.y - destination.back().position.y,
                point.position.z - destination.back().position.z }) > 1.0e-8f) {
            destination.push_back(point);
        }
    };
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

    const float safeLifeTime = (std::max)(lifeTime, 0.01f);
    const Vector3 axis = normalizeOr(exhaustDirection, { 0.0f, 0.0f, -1.0f });
    std::vector<TrailPoint> controls;
    controls.reserve(points_.size() + 2);
    // fixedFlameStart は現在のノズル口と同じ座標を受け取る。固定炎の先端を
    // 人工的な制御点として足すと、急旋回で「直線の炎→履歴」の角が残るため、
    // 光跡の曲線そのものはノズル口から始める。
    controls.push_back({ fixedFlameStart, 0.0f });
    appendPoint(controls, { headPosition_, 0.0f });
    for (const TrailPoint& point : points_) {
        if (point.age <= safeLifeTime) appendPoint(controls, point);
    }
    if (controls.size() < 3) return vertices;

    std::vector<TrailPoint> curvePoints;
    curvePoints.reserve(controls.size() * 3);
    curvePoints.push_back(controls.front());
    constexpr int kSubdivisions = 8;
    for (size_t index = 0; index + 1 < controls.size(); ++index) {
        const TrailPoint& p1 = controls[index];
        const TrailPoint& p2 = controls[index + 1];
        const Vector3 firstSegment = { p2.position.x - p1.position.x,
            p2.position.y - p1.position.y, p2.position.z - p1.position.z };
        const float virtualControlDistance = std::clamp(
            std::sqrt(lengthSquared(firstSegment)), 0.06f, 0.22f);
        // 先頭は「履歴点の延長」ではなく、現在のノズルの排気方向から仮想点を
        // 作る。これにより現在点→最初の履歴点も、ノズルの向きを接線とした
        // Catmull-Rom 曲線になる。
        const Vector3 p0 = index == 0
            ? Vector3{ p1.position.x - axis.x * virtualControlDistance,
                p1.position.y - axis.y * virtualControlDistance,
                p1.position.z - axis.z * virtualControlDistance }
            : controls[index - 1].position;
        const Vector3 p3 = index + 2 >= controls.size()
            ? Vector3{ p2.position.x * 2.0f - p1.position.x,
                p2.position.y * 2.0f - p1.position.y, p2.position.z * 2.0f - p1.position.z }
            : controls[index + 2].position;
        for (int step = 1; step <= kSubdivisions; ++step) {
            const float t = static_cast<float>(step) / kSubdivisions;
            appendPoint(curvePoints, { catmullRom(p0, p1.position, p2.position, p3, t),
                p1.age + (p2.age - p1.age) * t });
        }
    }
    // 光跡も立体炎と同じ centripetal Catmull-Rom 経路へ差し替える。
    curvePoints = BuildSpeedTrailCurve(fixedFlameStart, exhaustDirection, safeLifeTime);
    if (curvePoints.size() < 2) return vertices;

    struct RibbonSample {
        Vector3 position;
        Vector3 side;
        Vector3 toCamera;
        float ageRatio;
    };
    std::vector<RibbonSample> samples;
    samples.reserve(curvePoints.size());
    const Vector3 cameraPosition = camera->GetTranslate();
    Vector3 previousSide = { 0.0f, 0.0f, 0.0f };
    bool hasPreviousSide = false;
    for (size_t index = 0; index < curvePoints.size(); ++index) {
        const Vector3 tangent = normalizeOr(index == 0
            ? Vector3{ curvePoints[1].position.x - curvePoints[0].position.x,
                curvePoints[1].position.y - curvePoints[0].position.y,
                curvePoints[1].position.z - curvePoints[0].position.z }
            : (index + 1 == curvePoints.size()
                ? Vector3{ curvePoints[index].position.x - curvePoints[index - 1].position.x,
                    curvePoints[index].position.y - curvePoints[index - 1].position.y,
                    curvePoints[index].position.z - curvePoints[index - 1].position.z }
                : Vector3{ curvePoints[index + 1].position.x - curvePoints[index - 1].position.x,
                    curvePoints[index + 1].position.y - curvePoints[index - 1].position.y,
                    curvePoints[index + 1].position.z - curvePoints[index - 1].position.z }), axis);
        const Vector3 toCamera = normalizeOr({ cameraPosition.x - curvePoints[index].position.x,
            cameraPosition.y - curvePoints[index].position.y,
            cameraPosition.z - curvePoints[index].position.z }, { 0.0f, 1.0f, 0.0f });
        Vector3 side = MyMath::Cross(tangent, toCamera);
        if (lengthSquared(side) <= 1.0e-6f) side = MyMath::Cross(tangent, { 0.0f, 1.0f, 0.0f });
        side = normalizeOr(side, { 1.0f, 0.0f, 0.0f });
        // カメラ正対リボンでも、接線がカメラ方向をまたぐと左右が反転し得る。
        // 前の横軸と同じ半球へそろえる平行移動フレームにして、折れ目を防ぐ。
        if (hasPreviousSide && MyMath::Dot(side, previousSide) < 0.0f) {
            side = { -side.x, -side.y, -side.z };
        }
        previousSide = side;
        hasPreviousSide = true;
        samples.push_back({ curvePoints[index].position, side,
            toCamera, std::clamp(curvePoints[index].age / safeLifeTime, 0.0f, 1.0f) });
    }

    const auto appendLayer = [&](float widthScale, float alphaScale, const Vector4& color) {
        if (!vertices.empty()) {
            vertices.push_back(vertices.back());
            vertices.push_back(vertices.back());
        }
        for (const RibbonSample& sample : samples) {
            const float survivor = 1.0f - sample.ageRatio;
            const float halfWidth = thickness * widthScale * (0.10f + 0.90f * std::pow(survivor, 0.58f));
            const float alpha = intensity * alphaScale * color.w * std::pow(survivor, 1.35f);
            VertexData left{};
            left.position = { sample.position.x - sample.side.x * halfWidth,
                sample.position.y - sample.side.y * halfWidth,
                sample.position.z - sample.side.z * halfWidth, 1.0f };
            left.normal = { sample.toCamera.x, sample.toCamera.y, sample.toCamera.z, 0.0f };
            left.texcoord = { 0.0f, sample.ageRatio, 0.0f, 0.0f };
            left.color = { color.x, color.y, color.z, alpha };
            VertexData right = left;
            right.position = { sample.position.x + sample.side.x * halfWidth,
                sample.position.y + sample.side.y * halfWidth,
                sample.position.z + sample.side.z * halfWidth, 1.0f };
            right.texcoord.x = 1.0f;
            vertices.push_back(left);
            vertices.push_back(right);
        }
    };

    // 先に幅広く淡い外炎、次に白熱した細い芯を重ねる。
    appendLayer(2.65f, 0.34f, outerColor);
    appendLayer(1.10f, 0.92f, coreColor);
    return vertices;
}

std::vector<VertexData> Trail::GenerateCurvedThrusterVertices(Camera* camera, float thickness,
    const Vector3& nozzlePosition, const Vector3& exhaustDirection, float plumeLength,
    float phase, float intensity, float coreLengthRatio, float glowScale) const {
    std::vector<VertexData> vertices;
    if (!camera || thickness <= 0.0f || plumeLength <= 0.0f || intensity <= 0.01f) return vertices;

    const auto lengthSquared = [](const Vector3& value) {
        return value.x * value.x + value.y * value.y + value.z * value.z;
    };
    const auto normalizeOr = [&](const Vector3& value, const Vector3& fallback) {
        const float squared = lengthSquared(value);
        if (squared <= 1.0e-8f) return fallback;
        const float inverseLength = 1.0f / std::sqrt(squared);
        return Vector3{ value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
    };
    const Vector3 axis = normalizeOr(exhaustDirection, { 0.0f, 0.0f, -1.0f });
    const std::vector<TrailPoint> curve = BuildSpeedTrailCurve(nozzlePosition, exhaustDirection, lifeTime_);
    if (curve.size() < 2) return vertices;

    struct CurveSample {
        Vector3 position;
        float distance;
    };
    std::vector<CurveSample> samples;
    samples.reserve(curve.size());
    samples.push_back({ curve.front().position, 0.0f });
    for (size_t index = 1; index < curve.size(); ++index) {
        const Vector3 previous = samples.back().position;
        const Vector3 delta = { curve[index].position.x - previous.x,
            curve[index].position.y - previous.y, curve[index].position.z - previous.z };
        const float segmentLength = std::sqrt(lengthSquared(delta));
        if (segmentLength <= 1.0e-6f) continue;
        const float nextDistance = samples.back().distance + segmentLength;
        if (nextDistance >= plumeLength) {
            const float remaining = plumeLength - samples.back().distance;
            const float ratio = std::clamp(remaining / segmentLength, 0.0f, 1.0f);
            samples.push_back({ { previous.x + delta.x * ratio, previous.y + delta.y * ratio,
                previous.z + delta.z * ratio }, plumeLength });
            break;
        }
        samples.push_back({ curve[index].position, nextDistance });
    }
    if (samples.size() < 2) return vertices;

    struct Frame {
        Vector3 tangent;
        Vector3 normal;
        Vector3 binormal;
    };
    std::vector<Frame> frames;
    frames.reserve(samples.size());
    for (size_t index = 0; index < samples.size(); ++index) {
        const Vector3 tangent = normalizeOr(index == 0
            ? Vector3{ samples[1].position.x - samples[0].position.x,
                samples[1].position.y - samples[0].position.y,
                samples[1].position.z - samples[0].position.z }
            : (index + 1 == samples.size()
                ? Vector3{ samples[index].position.x - samples[index - 1].position.x,
                    samples[index].position.y - samples[index - 1].position.y,
                    samples[index].position.z - samples[index - 1].position.z }
                : Vector3{ samples[index + 1].position.x - samples[index - 1].position.x,
                    samples[index + 1].position.y - samples[index - 1].position.y,
                    samples[index + 1].position.z - samples[index - 1].position.z }), axis);
        Vector3 normal;
        if (frames.empty()) {
            const Vector3 reference = std::abs(tangent.y) < 0.9f
                ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
            normal = MyMath::Cross(tangent, reference);
        } else {
            // 前の断面軸を新しい接線へ平行移動し、カーブの途中でチューブが
            // 180度ねじれるのを防ぐ。
            const float projection = MyMath::Dot(frames.back().normal, tangent);
            normal = { frames.back().normal.x - tangent.x * projection,
                frames.back().normal.y - tangent.y * projection,
                frames.back().normal.z - tangent.z * projection };
        }
        if (lengthSquared(normal) <= 1.0e-6f) normal = MyMath::Cross(tangent, { 0.0f, 1.0f, 0.0f });
        normal = normalizeOr(normal, { 1.0f, 0.0f, 0.0f });
        const Vector3 binormal = normalizeOr(MyMath::Cross(tangent, normal), { 0.0f, 1.0f, 0.0f });
        frames.push_back({ tangent, normal, binormal });
    }

    struct Layer { float widthScale; float alpha; Vector4 color; };
    const Layer layers[] = {
        { 1.75f, 0.24f * std::clamp(glowScale, 0.0f, 3.0f), { 0.78f, 0.03f, 1.00f, 1.0f } },
        { 1.00f, 0.74f, { 1.00f, 0.04f, 0.55f, 1.0f } },
        { 0.48f, 0.92f, { 1.05f, 0.82f, 1.00f, 1.0f } }
    };
    constexpr int kSides = 10;
    constexpr float kPi = 3.14159265359f;
    const float totalLength = (std::max)(samples.back().distance, 1.0e-5f);
    vertices.reserve((samples.size() - 1) * kSides * 6 * 3);
    for (const Layer& layer : layers) {
        const auto makeVertex = [&](size_t sampleIndex, int side) {
            const float progress = samples[sampleIndex].distance / totalLength;
            const float profile = 0.15f + 0.85f * std::pow((std::max)(0.0f, 1.0f - progress), 0.62f);
            const float flicker = 1.0f + 0.012f * std::sin(phase + progress * 14.0f);
            const float radius = thickness * layer.widthScale * profile * flicker;
            const float angle = 2.0f * kPi * static_cast<float>(side % kSides) / kSides;
            const Vector3 radial = { frames[sampleIndex].normal.x * std::cos(angle) * radius +
                    frames[sampleIndex].binormal.x * std::sin(angle) * radius,
                frames[sampleIndex].normal.y * std::cos(angle) * radius +
                    frames[sampleIndex].binormal.y * std::sin(angle) * radius,
                frames[sampleIndex].normal.z * std::cos(angle) * radius +
                    frames[sampleIndex].binormal.z * std::sin(angle) * radius };
            const float tailFade = 1.0f - std::clamp((progress - 0.52f) / 0.48f, 0.0f, 1.0f);
            VertexData vertex{};
            vertex.position = { samples[sampleIndex].position.x + radial.x,
                samples[sampleIndex].position.y + radial.y, samples[sampleIndex].position.z + radial.z, 1.0f };
            const Vector3 surfaceNormal = normalizeOr(radial, frames[sampleIndex].normal);
            vertex.normal = { surfaceNormal.x, surfaceNormal.y, surfaceNormal.z, 0.0f };
            vertex.texcoord = { static_cast<float>(side % kSides) / kSides, progress, 0.0f, 0.0f };
            vertex.color = { layer.color.x, layer.color.y, layer.color.z,
                layer.alpha * intensity * tailFade };
            return vertex;
        };
        for (size_t segment = 0; segment + 1 < samples.size(); ++segment) {
            for (int side = 0; side < kSides; ++side) {
                const VertexData a = makeVertex(segment, side);
                const VertexData b = makeVertex(segment, side + 1);
                const VertexData c = makeVertex(segment + 1, side);
                const VertexData d = makeVertex(segment + 1, side + 1);
                vertices.insert(vertices.end(), { a, b, c, b, d, c });
            }
        }
    }
    (void)coreLengthRatio;
    return vertices;
}

std::vector<VertexData> Trail::GenerateThrusterFlareVertices(Camera* camera,
    const Vector3& center, float radius, float intensity) const {
    std::vector<VertexData> vertices;
    if (!camera || radius <= 0.0f || intensity <= 0.01f) return vertices;
    const auto normalizeOr = [](const Vector3& value, const Vector3& fallback) {
        const float squared = value.x * value.x + value.y * value.y + value.z * value.z;
        if (squared <= 1.0e-8f) return fallback;
        const float inverseLength = 1.0f / std::sqrt(squared);
        return Vector3{ value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
    };
    const Vector3 cameraPosition = camera->GetTranslate();
    const Vector3 toCamera = normalizeOr({ cameraPosition.x - center.x,
        cameraPosition.y - center.y, cameraPosition.z - center.z }, { 0.0f, 0.0f, 1.0f });
    Vector3 right = MyMath::Cross({ 0.0f, 1.0f, 0.0f }, toCamera);
    if (right.x * right.x + right.y * right.y + right.z * right.z <= 1.0e-8f) {
        right = MyMath::Cross({ 1.0f, 0.0f, 0.0f }, toCamera);
    }
    right = normalizeOr(right, { 1.0f, 0.0f, 0.0f });
    const Vector3 up = normalizeOr(MyMath::Cross(toCamera, right), { 0.0f, 1.0f, 0.0f });

    constexpr int kSides = 8;
    vertices.reserve(3 + (kSides - 1) * 5);
    const auto makeVertex = [&](const Vector3& position, float alpha) {
        VertexData vertex{};
        vertex.position = { position.x, position.y, position.z, 1.0f };
        vertex.normal = { toCamera.x, toCamera.y, toCamera.z, 0.0f };
        vertex.texcoord = { 0.5f, 0.0f, 0.0f, 0.0f };
        vertex.color = { 1.0f, 0.82f, 1.0f, alpha };
        return vertex;
    };
    std::vector<VertexData> perimeter;
    perimeter.reserve(kSides);
    constexpr float kPi = 3.14159265359f;
    for (int index = 0; index < kSides; ++index) {
        const float angle = 2.0f * kPi * static_cast<float>(index) / kSides;
        const Vector3 offset = { right.x * std::cos(angle) * radius + up.x * std::sin(angle) * radius,
            right.y * std::cos(angle) * radius + up.y * std::sin(angle) * radius,
            right.z * std::cos(angle) * radius + up.z * std::sin(angle) * radius };
        perimeter.push_back(makeVertex({ center.x + offset.x, center.y + offset.y, center.z + offset.z }, 0.0f));
    }
    const VertexData core = makeVertex(center, intensity);
    vertices.insert(vertices.end(), { core, perimeter[0], perimeter[1] });
    for (int index = 1; index < kSides; ++index) {
        const VertexData& next = perimeter[(index + 1) % kSides];
        vertices.insert(vertices.end(), { perimeter[index], core, core, perimeter[index], next });
    }
    return vertices;
}

std::vector<VertexData> Trail::GenerateThrusterVertices(Camera* camera, float thickness,
    const Vector3& exhaustDirection, float plumeLength, float phase, float intensity,
    float coreLengthRatio, float glowScale) {
    std::vector<VertexData> vertices;
    if (points_.empty() || thickness <= 0.0f || plumeLength <= 0.0f) return vertices;

    constexpr int kSegments = 32;
    // 視点に影響されない円形断面にする。炎の直径はどの角度から見ても
    // 同じで、カメラの回り込みで縦長・横長に変形しない。
    constexpr int kSides = 10;
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
    const Vector3 origin = points_.front().position;
    const Vector3 reference = std::abs(axis.y) < 0.9f
        ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
    const Vector3 radialAxisA = safeNormalize(MyMath::Cross(axis, reference));
    const Vector3 radialAxisB = safeNormalize(MyMath::Cross(axis, radialAxisA));
    (void)camera;

    struct Layer {
        float widthScale;
        float baseLengthScale;
        Vector4 emission;
    };
    // 全レイヤーは同じノズル口から始め、外側ほど少し長く残す。
    const Layer layers[] = {
        { 1.75f, 1.04f, { 0.78f, 0.03f, 1.0f, 0.24f * std::clamp(glowScale, 0.0f, 3.0f) } },
        { 1.00f, 1.00f, { 1.00f, 0.04f, 0.55f, 0.74f } },
        { 0.48f, std::clamp(coreLengthRatio, 0.2f, 0.8f), { 1.05f, 0.82f, 1.00f, 0.92f } }
    };
    vertices.reserve(kSegments * kSides * 6 * 3);
    intensity = std::clamp(intensity, 0.0f, 1.0f);

    for (const Layer& layer : layers) {
        const float length = (std::max)(plumeLength * layer.baseLengthScale, 0.01f);
        const auto widthAt = [&](float t) {
            // 根元をゼロ幅／ゼロ透明度にせず、ノズル径から後方へ一方向に絞る。
            // 末端は幅より先にアルファを落とすため、針状の輪郭も残らない。
            const float profile = 0.12f + 0.88f * std::pow((std::max)(0.0f, 1.0f - t), 0.62f);
            const float flicker = 1.0f + 0.012f * std::sin(phase + t * 14.0f);
            return thickness * layer.widthScale * profile * flicker;
        };
        const auto makeVertex = [&](float t, int side) {
            const float angle = 2.0f * kPi * static_cast<float>(side) / kSides;
            const float radius = widthAt(t);
            const Vector3 radialOffset = {
                radialAxisA.x * std::cos(angle) * radius + radialAxisB.x * std::sin(angle) * radius,
                radialAxisA.y * std::cos(angle) * radius + radialAxisB.y * std::sin(angle) * radius,
                radialAxisA.z * std::cos(angle) * radius + radialAxisB.z * std::sin(angle) * radius
            };
            const Vector3 center = {
                origin.x + axis.x * length * t,
                origin.y + axis.y * length * t,
                origin.z + axis.z * length * t
            };
            VertexData v{};
            v.position = { center.x + radialOffset.x, center.y + radialOffset.y,
                center.z + radialOffset.z, 1.0f };
            const Vector3 surfaceNormal = safeNormalize(radialOffset);
            v.normal = { surfaceNormal.x, surfaceNormal.y, surfaceNormal.z, 0.0f };
            v.texcoord = { static_cast<float>(side) / kSides, t, 0.0f, 0.0f };
            const float tailFade = 1.0f - smoothFade((t - 0.48f) / 0.52f);
            const float fade = tailFade * std::pow((std::max)(0.0f, 1.0f - t), 0.30f);
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
