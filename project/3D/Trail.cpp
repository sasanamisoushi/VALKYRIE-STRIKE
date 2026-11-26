#include "Trail.h"
#include "ThrusterTrailMeshBuilder.h"
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

std::vector<VertexData> Trail::GenerateSmokeTrailVertices(Camera* camera, float thickness, float alphaScale) const {
    std::vector<VertexData> vertices;
    if (!camera || points_.size() < 2) return vertices;
    const Vector3 camPos = camera->GetTranslate();
    for (size_t i = 0; i < points_.size(); ++i) {
        const float age = points_[i].age;
        // 過去の煙ほど上方向(+Y)へ立ち上りながら拡散する
        const Vector3 point = {
            points_[i].position.x,
            points_[i].position.y + age * 1.4f,
            points_[i].position.z,
        };
        Vector3 dir{};
        if (i < points_.size() - 1) {
            const Vector3 next = {
                points_[i + 1].position.x,
                points_[i + 1].position.y + points_[i + 1].age * 1.4f,
                points_[i + 1].position.z,
            };
            dir = { point.x - next.x, point.y - next.y, point.z - next.z };
        } else {
            const Vector3 previous = {
                points_[i - 1].position.x,
                points_[i - 1].position.y + points_[i - 1].age * 1.4f,
                points_[i - 1].position.z,
            };
            dir = { previous.x - point.x, previous.y - point.y, previous.z - point.z };
        }
        const float dirLenSq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
        dir = dirLenSq > 1.0e-6f ? MyMath::Normalize(dir) : Vector3{ 0.0f, 1.0f, 0.0f };

        Vector3 toCam = { camPos.x - point.x, camPos.y - point.y, camPos.z - point.z };
        const float toCamLenSq = toCam.x * toCam.x + toCam.y * toCam.y + toCam.z * toCam.z;
        toCam = toCamLenSq > 1.0e-6f ? MyMath::Normalize(toCam) : Vector3{ 0.0f, 0.0f, -1.0f };

        Vector3 right = MyMath::Cross(dir, toCam);
        const float rightLenSq = right.x * right.x + right.y * right.y + right.z * right.z;
        right = rightLenSq > 1.0e-6f ? MyMath::Normalize(right) : Vector3{ 1.0f, 0.0f, 0.0f };

        const float progress = 1.0f - (static_cast<float>(i) / static_cast<float>(points_.size() - 1));
        const float spread = std::pow(1.0f - progress, 1.15f);
        const float currentThickness = thickness * (0.45f + spread * 2.4f);
        const float noiseX = (std::sin(static_cast<float>(i) * 1.35f) * 0.25f) * spread;
        const float noiseY = (std::cos(static_cast<float>(i) * 1.15f) * 0.20f) * spread;
        const float noiseZ = (std::sin(static_cast<float>(i) * 0.95f) * 0.25f) * spread;
        const Vector3 leftPos = {
            point.x - right.x * currentThickness + noiseX,
            point.y - right.y * currentThickness + noiseY,
            point.z - right.z * currentThickness + noiseZ,
        };
        const Vector3 rightPos = {
            point.x + right.x * currentThickness + noiseX,
            point.y + right.y * currentThickness + noiseY,
            point.z + right.z * currentThickness + noiseZ,
        };
        const float headFade = std::clamp((1.0f - progress) * 6.0f, 0.0f, 1.0f);
        const float alpha = std::pow(progress, 1.35f) * headFade * alphaScale;
        const float vCoord = static_cast<float>(i) * 0.22f;

        VertexData vLeft{};
        vLeft.position = { leftPos.x, leftPos.y, leftPos.z, 1.0f };
        vLeft.normal = { toCam.x, toCam.y, toCam.z, 0.0f };
        vLeft.texcoord = { 0.0f, vCoord, 0.0f, 0.0f };
        vLeft.color = { 0.0f, 0.0f, 0.0f, alpha };

        VertexData vRight{};
        vRight.position = { rightPos.x, rightPos.y, rightPos.z, 1.0f };
        vRight.normal = { toCam.x, toCam.y, toCam.z, 0.0f };
        vRight.texcoord = { 1.0f, vCoord, 0.0f, 0.0f };
        vRight.color = { 0.0f, 0.0f, 0.0f, alpha };

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
    if (!camera || thickness <= 0.0f || plumeLength <= 0.0f || intensity <= 0.01f) {
        return {};
    }
    const std::vector<TrailPoint> curve = BuildSpeedTrailCurve(nozzlePosition, exhaustDirection, lifeTime_);
    std::vector<Vector3> curvePositions;
    curvePositions.reserve(curve.size());
    for (const TrailPoint& point : curve) {
        curvePositions.push_back(point.position);
    }
    return ThrusterTrailMeshBuilder::GenerateCurvedThrusterVertices(
        camera, thickness, curvePositions, exhaustDirection, plumeLength,
        phase, intensity, coreLengthRatio, glowScale);
}

std::vector<VertexData> Trail::GenerateThrusterFlareVertices(Camera* camera,
    const Vector3& center, float radius, float intensity) const {
    return ThrusterTrailMeshBuilder::GenerateThrusterFlareVertices(camera, center, radius, intensity);
}

std::vector<VertexData> Trail::GenerateThrusterVertices(Camera* camera, float thickness,
    const Vector3& exhaustDirection, float plumeLength, float phase, float intensity,
    float coreLengthRatio, float glowScale) {
    if (points_.empty()) {
        return {};
    }
    return ThrusterTrailMeshBuilder::GenerateThrusterVertices(
        camera, points_.front().position, thickness, exhaustDirection, plumeLength,
        phase, intensity, coreLengthRatio, glowScale);
}
