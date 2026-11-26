#include "ThrusterTrailMeshBuilder.h"
#include <algorithm>
#include <cmath>

std::vector<VertexData> ThrusterTrailMeshBuilder::GenerateCurvedThrusterVertices(Camera* camera,
    float thickness, const std::vector<Vector3>& curvePositions, const Vector3& exhaustDirection,
    float plumeLength, float phase, float intensity, float coreLengthRatio, float glowScale) {
    std::vector<VertexData> vertices;
    if (!camera || thickness <= 0.0f || plumeLength <= 0.0f || intensity <= 0.01f) return vertices;
    if (curvePositions.size() < 2) return vertices;

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

    struct CurveSample {
        Vector3 position;
        float distance;
    };
    std::vector<CurveSample> samples;
    samples.reserve(curvePositions.size());
    samples.push_back({ curvePositions.front(), 0.0f });
    for (size_t index = 1; index < curvePositions.size(); ++index) {
        const Vector3 previous = samples.back().position;
        const Vector3 delta = { curvePositions[index].x - previous.x,
            curvePositions[index].y - previous.y, curvePositions[index].z - previous.z };
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
        samples.push_back({ curvePositions[index], nextDistance });
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

std::vector<VertexData> ThrusterTrailMeshBuilder::GenerateThrusterFlareVertices(Camera* camera,
    const Vector3& center, float radius, float intensity) {
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

std::vector<VertexData> ThrusterTrailMeshBuilder::GenerateThrusterVertices(Camera* camera,
    const Vector3& origin, float thickness, const Vector3& exhaustDirection, float plumeLength,
    float phase, float intensity, float coreLengthRatio, float glowScale) {
    std::vector<VertexData> vertices;
    if (thickness <= 0.0f || plumeLength <= 0.0f) return vertices;

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
