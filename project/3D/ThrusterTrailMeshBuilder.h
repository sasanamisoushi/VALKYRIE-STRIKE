#pragma once
#include <vector>
#include "engine/math/MyMath.h"
#include "engine/Camera/Camera.h"
#include "3D/Model.h"

class ThrusterTrailMeshBuilder {
public:
    static std::vector<VertexData> GenerateCurvedThrusterVertices(Camera* camera, float thickness,
        const std::vector<Vector3>& curvePositions, const Vector3& exhaustDirection, float plumeLength,
        float phase, float intensity, float coreLengthRatio, float glowScale);

    static std::vector<VertexData> GenerateThrusterFlareVertices(Camera* camera,
        const Vector3& center, float radius, float intensity);

    static std::vector<VertexData> GenerateThrusterVertices(Camera* camera, const Vector3& origin,
        float thickness, const Vector3& exhaustDirection, float plumeLength, float phase,
        float intensity, float coreLengthRatio, float glowScale);
};
