#pragma once
#include "ModelCommon.h"
#include "MyMath.h"
#include "engine/Resource/TextureManager.h"
#include <memory>
#include <optional>
#include <map>
#include <string>
#include <vector>

struct aiNode;


struct VertexData {
	Vector4 position;
	Vector4 texcoord;
	Vector4 normal;
	Vector4 color;
	float weight[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	int32_t index[4] = { 0, 0, 0, 0 };
};

struct VertexWeightData {
	float weight;
	uint32_t vertexIndex;
};

struct JointWeightData {
	Matrix4x4 inverseBindMatrix;
	std::vector<VertexWeightData> vertexWeights;
};

struct WellKnownPalette {
	Matrix4x4 skeletonSpaceMatrix;
};

struct SkinningInformation {
	uint32_t numVertices;
	uint32_t numBones;
	uint32_t padding[62];
};

struct SkinCluster {
	std::vector<Matrix4x4> inverseBindMatrices;
	Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
	WellKnownPalette* mappedPalette = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS paletteAddress = 0;

	// GPU Skinning用
	Microsoft::WRL::ComPtr<ID3D12Resource> skinningInfoResource;
	SkinningInformation* mappedSkinningInfo = nullptr;

	// 入力頂点SRV、出力頂点UAV
	uint32_t srvIndexInputVertex;
	uint32_t srvIndexPalette;
	uint32_t uavIndexOutputVertex;

	std::vector<int32_t> boneIndexToJointIndex; // モデルのボーンIndexからSkeletonのJointIndexへのマッピング
	bool isValid = false;
	bool isUpdated = false; // フレーム内更新フラグ
};

struct MaterialData {
	std::string textureFilePath;
	uint32_t textureIndex = 0;
	float alphaReference = 0.0f;
};

#include "engine/Camera/Camera.h"

struct Node {
	QuaternionTransform transform;
	Matrix4x4 localMatrix;
	std::string name;
	std::vector<Node> children;
};

struct Joint {
    QuaternionTransform transform; // Transform情報
    Matrix4x4 localMatrix; // localMatrix
    Matrix4x4 skeletonSpaceMatrix; // skeletonSpaceでの変換行列
    std::string name; // 名前
    std::vector<int32_t> children; // 子JointのIndexのリスト。いなければ空
    int32_t index; // 自身のIndex
    std::optional<int32_t> parent; // 親JointのIndex。いなければnull
};

struct Skeleton {
    int32_t root; // RootJointのIndex
    std::map<std::string, int32_t> jointMap; // Joint名とIndexとの辞書
    std::vector<Joint> joints; // 所属しているジョイント
};

Skeleton CreateSkeleton(const Node& rootNode);
void Update(Skeleton& skeleton);

struct ModelData {
	std::vector<VertexData>vertices;
	std::vector<uint32_t> indices;
	MaterialData material;
	Node rootNode;
	std::map<std::string, JointWeightData> skinClusterData; // スキニング用のデータ
	std::vector<std::string> boneNames; // ボーンIndex順のジョイント名
	bool isSkinned = false; // スキニングモデルかどうか
	bool isLine = false; // ラインプリミティブかどうか
	bool isStrip = false; // ストリップ（帯）描画フラグ
};

struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
	float shininess;
	float alphaReference;
	float padding2[2];
};

class Model {
public:

	//初期化
	void Initialize(ModelCommon *modelCommon,const std::string& directorypath,const std::string& filename);

	//描画
	void Draw();

	//頂点データ作成
	void CreateVertexData();

	//インデックスデータ作成
	void CreateIndexData();

	//マテリアルデータ作成
	void CreateMaterialData();

	//.mtlファイルの読み込み
	static MaterialData LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename);

	// Nodeの読み取り
	static Node ReadNode(aiNode* node);
	
	// GLTF等からノード階層のみを読み取る
	static Node LoadNodeHierarchy(const std::string& directoryPath, const std::string& filename);

	//.objファイルの読み取り
	static ModelData LoadObjFile(const std::string &directoryPath, const std::string &filename);

	// .gltf等のメッシュ読み取り (Assimpを使用)
	static ModelData LoadGltfFile(const std::string &directoryPath, const std::string &filename);

	// 球の初期化
	void InitializeSphere(ModelCommon *modelCommon, int subdivision);

	// 平面の初期化
	void InitializePlane(ModelCommon *modelCommon);

	// ボックスの初期化
	void InitializeBox(ModelCommon *modelCommon);

	// リングの初期化
	void InitializeRing(ModelCommon *modelCommon, int subdivision = 32, float outerRadius = 1.0f, float innerRadius = 0.5f,
		bool isUvHorizontal = true, const Vector4& innerColor = { 1.0f, 1.0f, 1.0f, 1.0f }, const Vector4& outerColor = { 1.0f, 1.0f, 1.0f, 1.0f },
		float startAngleDegree = 0.0f, float endAngleDegree = 360.0f, float fadeAngleDegree = 0.0f);

	// 円柱（Cylinder）の初期化（拡張版）
	void InitializeCylinder(ModelCommon *modelCommon,
		int subdivision = 32,
		int verticalSubdivision = 1,
		float topRadiusX = 1.0f, float topRadiusZ = 1.0f,
		float bottomRadiusX = 1.0f, float bottomRadiusZ = 1.0f,
		float height = 3.0f,
		const Vector4& topColor = { 1.0f, 1.0f, 1.0f, 1.0f },
		const Vector4& bottomColor = { 1.0f, 1.0f, 1.0f, 1.0f },
		float startAngleDegree = 0.0f, float endAngleDegree = 360.0f,
		bool isUvFlipped = false);

	// ラインの初期化
	void InitializeLine(ModelCommon *modelCommon);
	void UpdateLineVertices(const std::vector<VertexData>& lines);

	void SetUvTransform(const Matrix4x4& transform) {
		if (materialData) {
			materialData->uvTransform = transform;
		}
	}

	void SetTextureFilePath(const std::string& path) {
		textureFilePath_ = path;
		modelData.material.textureFilePath = path;
		TextureManager::GetInstance()->LoadTexture(path);
		modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(path);
	}

	void SetAlphaReference(float alphaRef) {
		if (materialData) {
			materialData->alphaReference = alphaRef;
		}
		modelData.material.alphaReference = alphaRef;
	}

	SkinCluster CreateSkinCluster(const Skeleton& skeleton);
	void UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton);

	void SetColor(const Vector4& color) {
		if (materialData) {
			materialData->color = color;
		}
	}

	void SetEnableLighting(bool enable) {
		if (materialData) {
			materialData->enableLighting = enable ? 1 : 0;
		}
	}

	// スキニング
	bool Skinning(SkinCluster& skinCluster);

	const ModelData& GetModelData()const { return modelData; }

	ModelCommon* GetModelCommon() const { return modelCommon_; }

	// トレイル（帯）の初期化と頂点更新関数
	void InitializeTrail(ModelCommon *modelCommon);
	void UpdateTrailVertices(const std::vector<VertexData> &vertices, bool isTriangleStrip = true);

	Vector3 GetHalfExtents() const { return cachedHalfExtents_; }
	Vector3 GetBoundsCenter() const { return cachedBoundsCenter_; }

private:
	void RecalculateBounds();

	ModelCommon *modelCommon_;

	std::unique_ptr<MyMath> math = std::make_unique<MyMath>();

	//Objファイルのデータ
	ModelData modelData;
	Vector3 cachedHalfExtents_ = { 1.0f, 1.0f, 1.0f };
	Vector3 cachedBoundsCenter_ = { 0.0f, 0.0f, 0.0f };

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	//スキニング用入力頂点リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> inputVertexResource;
	//バッファリソース内のデータを指すポインタ
	VertexData *vertexData = nullptr;
	//バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	//インデックスバッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	//インデックスバッファビュー
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

	//マテリアルリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> matetialResource;
	//バッファリソース内のデータを指すポイント
	Material *materialData = nullptr;

	// テクスチャファイルパス
	std::string textureFilePath_;

};

