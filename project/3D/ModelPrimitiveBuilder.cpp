#include "ModelPrimitiveBuilder.h"
#include <algorithm>
#include <cmath>

std::vector<VertexData> ModelPrimitiveBuilder::BuildSphereVertices(int subdivision) {
	// 球の頂点を計算するための変数
	const float pi = 3.1415926535f;
	const float latEvery = pi / subdivision;
	const float lonEvery = (2.0f * pi) / subdivision;

	std::vector<VertexData> tempVertices;

	// 球面上に頂点を計算
	for (int latIndex = 0; latIndex <= subdivision; ++latIndex) {
		float lat = -pi / 2.0f + latEvery * latIndex;
		for (int lonIndex = 0; lonIndex <= subdivision; ++lonIndex) {
			float lon = lonEvery * lonIndex;
			VertexData vertex{};

			// 法線と座標の計算
			vertex.normal.x = std::cos(lat) * std::cos(lon);
			vertex.normal.y = std::sin(lat);
			vertex.normal.z = std::cos(lat) * std::sin(lon);
			vertex.position = { vertex.normal.x, vertex.normal.y, vertex.normal.z, 1.0f };

			// UV座標の計算
			vertex.texcoord = { float(lonIndex) / subdivision, 1.0f - float(latIndex) / subdivision };

			// 頂点カラーのデフォルト
			vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			tempVertices.push_back(vertex);
		}
	}

	// 三角形リストとして展開
	std::vector<VertexData> vertices;
	for (int latIndex = 0; latIndex < subdivision; ++latIndex) {
		for (int lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
			int start = (latIndex * (subdivision + 1)) + lonIndex;

			int a = start;
			int b = start + 1;
			int c = start + (subdivision + 1);
			int d = start + (subdivision + 1) + 1;

			// 三角形1
			vertices.push_back(tempVertices[a]);
			vertices.push_back(tempVertices[c]);
			vertices.push_back(tempVertices[b]);

			// 三角形2
			vertices.push_back(tempVertices[b]);
			vertices.push_back(tempVertices[c]);
			vertices.push_back(tempVertices[d]);
		}
	}
	return vertices;
}

std::vector<VertexData> ModelPrimitiveBuilder::BuildPlaneVertices() {
	// 平面を構成する4つの頂点（XY平面、幅2、高さ2）
	VertexData v[4];
	// 左下
	v[0].position = { -1.0f, -1.0f, 0.0f, 1.0f };
	v[0].texcoord = { 0.0f, 1.0f };
	v[0].normal = { 0.0f, 0.0f, -1.0f };
	v[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 左上
	v[1].position = { -1.0f, 1.0f, 0.0f, 1.0f };
	v[1].texcoord = { 0.0f, 0.0f };
	v[1].normal = { 0.0f, 0.0f, -1.0f };
	v[1].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 右下
	v[2].position = { 1.0f, -1.0f, 0.0f, 1.0f };
	v[2].texcoord = { 1.0f, 1.0f };
	v[2].normal = { 0.0f, 0.0f, -1.0f };
	v[2].color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 右上
	v[3].position = { 1.0f, 1.0f, 0.0f, 1.0f };
	v[3].texcoord = { 1.0f, 0.0f };
	v[3].normal = { 0.0f, 0.0f, -1.0f };
	v[3].color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 三角形リストとして展開（インデックスを使わない書き方）
	return {
		v[0], v[1], v[2], // 三角形1（左下、左上、右下）
		v[1], v[3], v[2]  // 三角形2（左上、右上、右下）
	};
}

std::vector<VertexData> ModelPrimitiveBuilder::BuildBoxVertices() {
	std::vector<VertexData> vertices;

	// 箱の角になる8つの頂点座標（幅2、高さ2、奥行き2）
	Vector4 p[8] = {
		{-1.0f, -1.0f, -1.0f, 1.0f}, // 0: 左下の手前
		{-1.0f,  1.0f, -1.0f, 1.0f}, // 1: 左上の手前
		{ 1.0f, -1.0f, -1.0f, 1.0f}, // 2: 右下の手前
		{ 1.0f,  1.0f, -1.0f, 1.0f}, // 3: 右上の手前
		{-1.0f, -1.0f,  1.0f, 1.0f}, // 4: 左下の奥
		{-1.0f,  1.0f,  1.0f, 1.0f}, // 5: 左上の奥
		{ 1.0f, -1.0f,  1.0f, 1.0f}, // 6: 右下の奥
		{ 1.0f,  1.0f,  1.0f, 1.0f}  // 7: 右上の奥
	};

	// 6つの面が向いている方向（法線）
	Vector3 nFront = { 0.0f,  0.0f, -1.0f };
	Vector3 nBack = { 0.0f,  0.0f,  1.0f };
	Vector3 nLeft = { -1.0f,  0.0f,  0.0f };
	Vector3 nRight = { 1.0f,  0.0f,  0.0f };
	Vector3 nTop = { 0.0f,  1.0f,  0.0f };
	Vector3 nBottom = { 0.0f, -1.0f,  0.0f };

	// テクスチャのUV座標
	Vector2 uv00 = { 0.0f, 1.0f };
	Vector2 uv01 = { 0.0f, 0.0f };
	Vector2 uv10 = { 1.0f, 1.0f };
	Vector2 uv11 = { 1.0f, 0.0f };

	// 面（四角形＝三角形2枚）を作る便利関数
	auto addFace = [&](int i0, int i1, int i2, int i3, Vector3 n) {
		Vector4 c = { 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 norm = { n.x, n.y, n.z, 0.0f };
		vertices.push_back({ p[i0], {uv00.x, uv00.y, 0.0f, 0.0f}, norm, c });
		vertices.push_back({ p[i1], {uv01.x, uv01.y, 0.0f, 0.0f}, norm, c });
		vertices.push_back({ p[i2], {uv10.x, uv10.y, 0.0f, 0.0f}, norm, c });

		vertices.push_back({ p[i1], {uv01.x, uv01.y, 0.0f, 0.0f}, norm, c });
		vertices.push_back({ p[i3], {uv11.x, uv11.y, 0.0f, 0.0f}, norm, c });
		vertices.push_back({ p[i2], {uv10.x, uv10.y, 0.0f, 0.0f}, norm, c });
	};

	// 前, 奥, 左, 右, 上, 下の6面を作る
	addFace(0, 1, 2, 3, nFront);
	addFace(6, 7, 4, 5, nBack);
	addFace(4, 5, 0, 1, nLeft);
	addFace(2, 3, 6, 7, nRight);
	addFace(1, 5, 3, 7, nTop);
	addFace(4, 0, 6, 2, nBottom);
	return vertices;
}

std::vector<VertexData> ModelPrimitiveBuilder::BuildRingVertices(
	int subdivision, float outerRadius, float innerRadius,
	bool isUvHorizontal, const Vector4& innerColor, const Vector4& outerColor,
	float startAngleDegree, float endAngleDegree, float fadeAngleDegree) {
	const float pi = 3.1415926535f;

	// 角度の補正（終了角度が開始角度以下なら360度足すなど）
	if (endAngleDegree <= startAngleDegree) endAngleDegree += 360.0f;

	float startRad = startAngleDegree * pi / 180.0f;
	float endRad = endAngleDegree * pi / 180.0f;
	float fadeRad = fadeAngleDegree * pi / 180.0f;
	float totalRad = endRad - startRad;

	std::vector<VertexData> tempVertices;

	// リングの頂点を計算
	for (int i = 0; i <= subdivision; ++i) {
		float t = (float)i / subdivision;
		float theta = startRad + t * totalRad;
		float c = std::cos(theta);
		float s = std::sin(theta);

		// アルファフェードの計算
		float alpha = 1.0f;
		if (fadeRad > 0.0f) {
			float distFromStart = theta - startRad;
			float distFromEnd = endRad - theta;
			float minEdgeDist = (std::min)(distFromStart, distFromEnd);
			if (minEdgeDist < fadeRad) {
				alpha = minEdgeDist / fadeRad;
			}
		}

		Vector4 outColor = { outerColor.x, outerColor.y, outerColor.z, outerColor.w * alpha };
		Vector4 inColor = { innerColor.x, innerColor.y, innerColor.z, innerColor.w * alpha };

		// テクスチャ座標の計算
		float uv_u = isUvHorizontal ? t : 0.0f;
		float uv_v = isUvHorizontal ? 0.0f : t;

		// 外側の頂点
		VertexData vOuter;
		vOuter.position = { outerRadius * c, outerRadius * s, 0.0f, 1.0f };
		vOuter.texcoord = { uv_u, uv_v };
		vOuter.normal = { 0.0f, 0.0f, -1.0f };
		vOuter.color = outColor;

		// 内側の頂点
		VertexData vInner;
		vInner.position = { innerRadius * c, innerRadius * s, 0.0f, 1.0f };
		vInner.texcoord = { isUvHorizontal ? t : 1.0f, isUvHorizontal ? 1.0f : t };
		vInner.normal = { 0.0f, 0.0f, -1.0f };
		vInner.color = inColor;

		tempVertices.push_back(vOuter); // index 2*i
		tempVertices.push_back(vInner); // index 2*i + 1
	}

	// 三角形リストとして展開
	// 前面から見て時計回り（左手座標系の標準）になるように頂点順序を設定
	std::vector<VertexData> vertices;
	for (int i = 0; i < subdivision; ++i) {
		int outer0 = 2 * i;
		int inner0 = 2 * i + 1;
		int outer1 = 2 * (i + 1);
		int inner1 = 2 * (i + 1) + 1;

		// 三角形1: outer0 -> inner0 -> inner1
		vertices.push_back(tempVertices[outer0]);
		vertices.push_back(tempVertices[inner0]);
		vertices.push_back(tempVertices[inner1]);

		// 三角形2: outer0 -> inner1 -> outer1
		vertices.push_back(tempVertices[outer0]);
		vertices.push_back(tempVertices[inner1]);
		vertices.push_back(tempVertices[outer1]);
	}
	return vertices;
}

std::vector<VertexData> ModelPrimitiveBuilder::BuildCylinderVertices(
	int subdivision, int verticalSubdivision,
	float topRadiusX, float topRadiusZ,
	float bottomRadiusX, float bottomRadiusZ,
	float height,
	const Vector4& topColor, const Vector4& bottomColor,
	float startAngleDegree, float endAngleDegree,
	bool isUvFlipped) {
	const float pi = 3.1415926535f;
	if (endAngleDegree <= startAngleDegree) endAngleDegree += 360.0f;

	float startRad = startAngleDegree * pi / 180.0f;
	float endRad = endAngleDegree * pi / 180.0f;
	float totalRad = endRad - startRad;

	// 頂点格子を生成
	std::vector<std::vector<VertexData>> grid(verticalSubdivision + 1, std::vector<VertexData>(subdivision + 1));

	for (int v = 0; v <= verticalSubdivision; ++v) {
		float vt = (float)v / verticalSubdivision;
		float y = height * vt;
		float radiusX = bottomRadiusX + (topRadiusX - bottomRadiusX) * vt;
		float radiusZ = bottomRadiusZ + (topRadiusZ - bottomRadiusZ) * vt;
		Vector4 color = {
			bottomColor.x + (topColor.x - bottomColor.x) * vt,
			bottomColor.y + (topColor.y - bottomColor.y) * vt,
			bottomColor.z + (topColor.z - bottomColor.z) * vt,
			bottomColor.w + (topColor.w - bottomColor.w) * vt
		};

		for (int h = 0; h <= subdivision; ++h) {
			float ht = (float)h / subdivision;
			float theta = startRad + ht * totalRad;
			float sin = std::sin(theta);
			float cos = std::cos(theta);

			VertexData vertex;
			vertex.position = { -sin * radiusX, y, cos * radiusZ, 1.0f };

			float u = ht;
			float v_coord = isUvFlipped ? vt : (1.0f - vt);
			vertex.texcoord = { u, v_coord };

			// 法線は外向き
			vertex.normal = { -sin, 0.0f, cos };
			vertex.color = color;

			grid[v][h] = vertex;
		}
	}

	// 三角形リストとして展開
	std::vector<VertexData> vertices;
	for (int v = 0; v < verticalSubdivision; ++v) {
		for (int h = 0; h < subdivision; ++h) {
			const VertexData& v0 = grid[v][h];
			const VertexData& v1 = grid[v + 1][h];
			const VertexData& v2 = grid[v][h + 1];
			const VertexData& v3 = grid[v + 1][h + 1];

			// 三角形1: v0 -> v1 -> v3
			vertices.push_back(v0);
			vertices.push_back(v3);
			vertices.push_back(v1);

			// 三角形2: v0 -> v3 -> v2
			vertices.push_back(v0);
			vertices.push_back(v2);
			vertices.push_back(v3);
		}
	}
	return vertices;
}

std::vector<VertexData> ModelPrimitiveBuilder::BuildCylinderShellVertices(
	int subdivision, int verticalSubdivision,
	float radius, float height, float thickness,
	float startAngleDegree, float endAngleDegree) {
	subdivision = (std::max)(1, subdivision);
	verticalSubdivision = (std::max)(1, verticalSubdivision);
	const float safeThickness = (std::max)(0.001f, thickness);
	const float outerRadius = (std::max)(0.001f, radius + safeThickness * 0.5f);
	const float innerRadius = (std::max)(0.001f, radius - safeThickness * 0.5f);
	if (endAngleDegree <= startAngleDegree) endAngleDegree += 360.0f;

	constexpr float kPi = 3.1415926535f;
	const float startRadians = startAngleDegree * kPi / 180.0f;
	const float totalRadians = (endAngleDegree - startAngleDegree) * kPi / 180.0f;

	std::vector<VertexData> vertices;
	auto makeVertex = [](const Vector3& position, const Vector3& normal, float u, float v) {
		VertexData vertex{};
		vertex.position = { position.x, position.y, position.z, 1.0f };
		vertex.normal = { normal.x, normal.y, normal.z, 0.0f };
		vertex.texcoord = { u, v, 0.0f, 0.0f };
		vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		return vertex;
	};
	auto appendQuad = [&vertices](const VertexData& a, const VertexData& b,
		const VertexData& c, const VertexData& d) {
		vertices.push_back(a);
		vertices.push_back(b);
		vertices.push_back(c);
		vertices.push_back(a);
		vertices.push_back(c);
		vertices.push_back(d);
	};

	std::vector<std::vector<VertexData>> outer(verticalSubdivision + 1,
		std::vector<VertexData>(subdivision + 1));
	std::vector<std::vector<VertexData>> inner(verticalSubdivision + 1,
		std::vector<VertexData>(subdivision + 1));
	for (int vertical = 0; vertical <= verticalSubdivision; ++vertical) {
		const float verticalRatio = static_cast<float>(vertical) / verticalSubdivision;
		const float y = height * verticalRatio;
		for (int horizontal = 0; horizontal <= subdivision; ++horizontal) {
			const float horizontalRatio = static_cast<float>(horizontal) / subdivision;
			const float angle = startRadians + totalRadians * horizontalRatio;
			const float sine = std::sin(angle);
			const float cosine = std::cos(angle);
			const Vector3 outerNormal = { -sine, 0.0f, cosine };
			outer[vertical][horizontal] = makeVertex(
				{ outerNormal.x * outerRadius, y, outerNormal.z * outerRadius },
				outerNormal, horizontalRatio, 1.0f - verticalRatio);
			inner[vertical][horizontal] = makeVertex(
				{ outerNormal.x * innerRadius, y, outerNormal.z * innerRadius },
				{ -outerNormal.x, 0.0f, -outerNormal.z }, 1.0f - horizontalRatio, 1.0f - verticalRatio);
		}
	}

	// 外面と内面。内面は巻き順を反転して、内側から正面として描画されるようにする。
	for (int vertical = 0; vertical < verticalSubdivision; ++vertical) {
		for (int horizontal = 0; horizontal < subdivision; ++horizontal) {
			appendQuad(outer[vertical][horizontal], outer[vertical + 1][horizontal],
				outer[vertical + 1][horizontal + 1], outer[vertical][horizontal + 1]);
			appendQuad(inner[vertical][horizontal + 1], inner[vertical + 1][horizontal + 1],
				inner[vertical + 1][horizontal], inner[vertical][horizontal]);
		}
	}

	// 円弧の左右端を塞ぐ。ここが見えることで真横からも厚みを認識できる。
	for (int vertical = 0; vertical < verticalSubdivision; ++vertical) {
		for (int end = 0; end < 2; ++end) {
			const int horizontal = end == 0 ? 0 : subdivision;
			const float angle = startRadians + totalRadians * (end == 0 ? 0.0f : 1.0f);
			const Vector3 capNormal = end == 0
				? Vector3{ std::cos(angle), 0.0f, std::sin(angle) }
				: Vector3{ -std::cos(angle), 0.0f, -std::sin(angle) };
			auto makeCapVertex = [&](const VertexData& source) {
				return makeVertex({ source.position.x, source.position.y, source.position.z }, capNormal,
					source.texcoord.x, source.texcoord.y);
			};
			if (end == 0) {
				appendQuad(makeCapVertex(inner[vertical][horizontal]), makeCapVertex(inner[vertical + 1][horizontal]),
					makeCapVertex(outer[vertical + 1][horizontal]), makeCapVertex(outer[vertical][horizontal]));
			} else {
				appendQuad(makeCapVertex(outer[vertical][horizontal]), makeCapVertex(outer[vertical + 1][horizontal]),
					makeCapVertex(inner[vertical + 1][horizontal]), makeCapVertex(inner[vertical][horizontal]));
			}
		}
	}

	// 上面・下面も閉じ、斜め上や斜め下から見ても中空に見えないようにする。
	for (int horizontal = 0; horizontal < subdivision; ++horizontal) {
		const Vector3 topNormal = { 0.0f, 1.0f, 0.0f };
		const Vector3 bottomNormal = { 0.0f, -1.0f, 0.0f };
		auto makeCapVertex = [&](const VertexData& source, const Vector3& normal) {
			return makeVertex({ source.position.x, source.position.y, source.position.z }, normal,
				source.texcoord.x, source.texcoord.y);
		};
		appendQuad(makeCapVertex(outer[verticalSubdivision][horizontal], topNormal),
			makeCapVertex(inner[verticalSubdivision][horizontal], topNormal),
			makeCapVertex(inner[verticalSubdivision][horizontal + 1], topNormal),
			makeCapVertex(outer[verticalSubdivision][horizontal + 1], topNormal));
		appendQuad(makeCapVertex(inner[0][horizontal], bottomNormal),
			makeCapVertex(outer[0][horizontal], bottomNormal),
			makeCapVertex(outer[0][horizontal + 1], bottomNormal),
			makeCapVertex(inner[0][horizontal + 1], bottomNormal));
	}

	return vertices;
}
