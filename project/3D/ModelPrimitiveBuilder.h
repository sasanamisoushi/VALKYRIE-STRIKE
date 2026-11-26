#pragma once
#include "Model.h"
#include <vector>

class ModelPrimitiveBuilder {
public:
	static std::vector<VertexData> BuildSphereVertices(int subdivision);
	static std::vector<VertexData> BuildPlaneVertices();
	static std::vector<VertexData> BuildBoxVertices();
	static std::vector<VertexData> BuildRingVertices(
		int subdivision, float outerRadius, float innerRadius,
		bool isUvHorizontal, const Vector4& innerColor, const Vector4& outerColor,
		float startAngleDegree, float endAngleDegree, float fadeAngleDegree);
	static std::vector<VertexData> BuildCylinderVertices(
		int subdivision, int verticalSubdivision,
		float topRadiusX, float topRadiusZ,
		float bottomRadiusX, float bottomRadiusZ,
		float height,
		const Vector4& topColor, const Vector4& bottomColor,
		float startAngleDegree, float endAngleDegree,
		bool isUvFlipped);
	static std::vector<VertexData> BuildCylinderShellVertices(
		int subdivision, int verticalSubdivision,
		float radius, float height, float thickness,
		float startAngleDegree, float endAngleDegree);
};
