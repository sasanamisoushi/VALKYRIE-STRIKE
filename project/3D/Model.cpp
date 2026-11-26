#include "Model.h"
#include "ModelFileLoader.h"
#include "ModelPrimitiveBuilder.h"
#include "Object3dCommon.h"
#include "ModelCommon.h"
#include "engine/Resource/TextureManager.h"
#include "engine/Graphics/SrvManager.h"
#include <algorithm>
#include <string>
#include <utility>
#include <wrl.h>
#include <cmath>
#include "engine/base/Logger.h"

using Microsoft::WRL::ComPtr;

void SkinCluster::ReleaseDescriptorIndices() noexcept {
	SrvManager* srvManager = SrvManager::GetInstance();
	auto release = [srvManager](uint32_t& index) {
		if (index != UINT32_MAX) {
			srvManager->Release(index);
			index = UINT32_MAX;
		}
	};
	release(srvIndexInputVertex);
	release(srvIndexPalette);
	release(uavIndexOutputVertex);
}

SkinCluster::~SkinCluster() {
	ReleaseDescriptorIndices();
}

SkinCluster::SkinCluster(SkinCluster&& other) noexcept {
	*this = std::move(other);
}

SkinCluster& SkinCluster::operator=(SkinCluster&& other) noexcept {
	if (this == &other) {
		return *this;
	}

	ReleaseDescriptorIndices();
	inverseBindMatrices = std::move(other.inverseBindMatrices);
	paletteResource = std::move(other.paletteResource);
	mappedPalette = other.mappedPalette;
	paletteAddress = other.paletteAddress;
	skinningInfoResource = std::move(other.skinningInfoResource);
	mappedSkinningInfo = other.mappedSkinningInfo;
	srvIndexInputVertex = other.srvIndexInputVertex;
	srvIndexPalette = other.srvIndexPalette;
	uavIndexOutputVertex = other.uavIndexOutputVertex;
	boneIndexToJointIndex = std::move(other.boneIndexToJointIndex);
	isValid = other.isValid;
	isUpdated = other.isUpdated;

	other.mappedPalette = nullptr;
	other.paletteAddress = 0;
	other.mappedSkinningInfo = nullptr;
	other.srvIndexInputVertex = UINT32_MAX;
	other.srvIndexPalette = UINT32_MAX;
	other.uavIndexOutputVertex = UINT32_MAX;
	other.isValid = false;
	other.isUpdated = false;
	return *this;
}

namespace {

bool UploadVertexData(ID3D12Resource* resource, const std::vector<VertexData>& vertices) {
	if (!resource || vertices.empty()) {
		return false;
	}

	VertexData* mappedData = nullptr;
	HRESULT hr = resource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
	if (FAILED(hr) || mappedData == nullptr) {
		return false;
	}

	std::memcpy(mappedData, vertices.data(), sizeof(VertexData) * vertices.size());
	resource->Unmap(0, nullptr);
	return true;
}

}

void Model::Initialize(ModelCommon *modelCommon, const std::string &directorypath, const std::string &filename) {

	//引数で受け取ってメンバ変数に記録する
	this->modelCommon_ = modelCommon;

	//モデルの読み込み
	if (filename.find(".gltf") != std::string::npos || filename.find(".GLTF") != std::string::npos) {
		modelData = LoadGltfFile(directorypath, filename);
	} else {
		modelData = LoadObjFile(directorypath, filename);
	}

	// ロード失敗時のフォールバック (頂点が空ならダミーを作る)
	if (modelData.vertices.empty()) {
		VertexData v;
		v.position = { 0, 0, 0, 1 }; v.color = { 1, 0, 0, 1 }; modelData.vertices.push_back(v);
		v.position = { 0, 1, 0, 1 }; v.color = { 0, 1, 0, 1 }; modelData.vertices.push_back(v);
		v.position = { 1, 0, 0, 1 }; v.color = { 0, 0, 1, 1 }; modelData.vertices.push_back(v);
	}

	if (modelData.material.textureFilePath.empty()) {
		modelData.material.textureFilePath = "resources/white1x1.png";
	}


	//頂点データ作成
	CreateVertexData();

	//インデックスデータ作成
	CreateIndexData();

	//マテリアルデータ作成
	CreateMaterialData();


	//.objの参照しているテクスチャファイル読み込み
	TextureManager::GetInstance()->LoadTexture(modelData.material.textureFilePath);
	//読み込んだテクスチャの番号を取得
	modelData.material.textureIndex =
		TextureManager::GetInstance()->GetTextureIndexByFilePath(modelData.material.textureFilePath);

	// テクスチャファイルパス
	//textureFilePath_= filename;

	textureFilePath_ = modelData.material.textureFilePath;
	RecalculateBounds();
	logger.Log(" [Model] Loaded: " + filename + " | isSkinned: " + (modelData.isSkinned ? "true" : "false") + " | vertices: " + std::to_string(modelData.vertices.size()) + " | center: (" + std::to_string(cachedBoundsCenter_.x) + ", " + std::to_string(cachedBoundsCenter_.y) + ", " + std::to_string(cachedBoundsCenter_.z) + ") | extents: (" + std::to_string(cachedHalfExtents_.x) + ", " + std::to_string(cachedHalfExtents_.y) + ", " + std::to_string(cachedHalfExtents_.z) + ")");
}


void Model::Draw() {
	auto commandList = modelCommon_->GetDxCommon()->GetCommandList();
	if (modelData.vertices.empty() || !vertexResource) {
		return;
	}

	modelCommon_->GetDxCommon()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	if (!modelData.indices.empty()) {
		modelCommon_->GetDxCommon()->GetCommandList()->IASetIndexBuffer(&indexBufferView);
	}

	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけばよい
	if (modelData.isLine) {
		modelCommon_->GetDxCommon()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	} else if (modelData.isStrip) {
		// トレイル描画用の連続三角形モード
		modelCommon_->GetDxCommon()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	} else {
		modelCommon_->GetDxCommon()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	//マテリアルCBufferの場所を設定
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(0, matetialResource->GetGPUVirtualAddress());

	//SRVのDescriptorの先頭を設定
	modelCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
	
	if (!modelData.indices.empty()) {
		modelCommon_->GetDxCommon()->GetCommandList()->DrawIndexedInstanced(UINT(modelData.indices.size()), 1, 0, 0, 0);
	} else {
		modelCommon_->GetDxCommon()->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
	}
}


void Model::CreateVertexData() {
	size_t vertexBufferSize = sizeof(VertexData) * modelData.vertices.size();
	vertexBufferView = {};

	if (modelData.vertices.empty() || vertexBufferSize == 0) {
		vertexResource.Reset();
		inputVertexResource.Reset();
		return;
	}

	if (modelData.isSkinned) {
		// スキニング用: 変形後の頂点を入れるバッファ (UAV/描画用, DEFAULTヒープ)
		vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(vertexBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		
		// スキニング用: 変形前の頂点を入れるバッファ (SRV用, UPLOADヒープ)
		inputVertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(vertexBufferSize);
		if (!vertexResource || !UploadVertexData(inputVertexResource.Get(), modelData.vertices)) {
			vertexResource.Reset();
			inputVertexResource.Reset();
			return;
		}

		// 初期状態としてバリアを張っておく
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = vertexResource.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		modelCommon_->GetDxCommon()->GetCommandList()->ResourceBarrier(1, &barrier);

		// 初期ポーズを vertexResource にコピーしておく (表示確認のため)
		// 本来は DEFAULTヒープへのコピーは中間リソースが必要だが、
		// スキニングCSが走れば上書きされるので、一旦ここでは「何もしない」のではなく
		// 「バリアだけ確実に張る」ことを徹底する。
	} else {
		// 通常モデルは従来通り UPLOADヒープ
		vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(vertexBufferSize);
		inputVertexResource = nullptr;
		
		if (!UploadVertexData(vertexResource.Get(), modelData.vertices)) {
			vertexResource.Reset();
			return;
		}
	}

	//バッファビューの設定
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = UINT(vertexBufferSize);
	vertexBufferView.StrideInBytes = sizeof(VertexData);
}

void Model::CreateIndexData() {
	if (modelData.indices.empty()) {
		return;
	}

	indexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * modelData.indices.size());

	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * modelData.indices.size());
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	uint32_t* mappedIndex = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	std::memcpy(mappedIndex, modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());
	indexResource->Unmap(0, nullptr);
}

void Model::CreateMaterialData() {
	matetialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));

	//書き込むためのアドレスを取得
	matetialResource->Map(0, nullptr, reinterpret_cast<void **>(&materialData));
	//今回は赤を書き込んで見る
	materialData->color = modelData.material.color;
	materialData->enableLighting = 1;
	materialData->doubleSidedLighting = 0;
	materialData->uvTransform = math->MakeIdentity4x4();

	materialData->shininess = 40.0f;
	materialData->alphaReference = modelData.material.alphaReference;
}

MaterialData Model::LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename) {
	return ModelFileLoader::LoadMaterialTemplateFile(directoryPath, filename);
}

ModelData Model::LoadObjFile(const std::string &directoryPath, const std::string &filename) {
	return ModelFileLoader::LoadObjFile(directoryPath, filename);
}

ModelData Model::LoadGltfFile(const std::string &directoryPath, const std::string &filename) {
	return ModelFileLoader::LoadGltfFile(directoryPath, filename);
}

Node Model::ReadNode(aiNode* node) {
	return ModelFileLoader::ReadNode(node);
}

Node Model::LoadNodeHierarchy(const std::string& directoryPath, const std::string& filename) {
	return ModelFileLoader::LoadNodeHierarchy(directoryPath, filename);
}

int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
    Joint joint;
    joint.name = node.name;
    joint.localMatrix = node.localMatrix;
    joint.skeletonSpaceMatrix = MyMath::MakeIdentity4x4();
    joint.transform = node.transform;
    joint.index = int32_t(joints.size()); // 現在登録されている数をIndexに
    joint.parent = parent;
    joints.push_back(joint); // SkeletonのJoint列に追加
    for (const Node& child : node.children) {
		// 子Jointを作成し、そのIndexを登録
        int32_t childIndex = CreateJoint(child, joint.index, joints);
        joints[joint.index].children.push_back(childIndex);
    }
    // 自身のIndexを返す
    return joint.index;
}

void Update(Skeleton& skeleton) {
	// すべてのJointを更新。親が先なので通常ループで処理可能になっている
    for (Joint& joint : skeleton.joints) {
        joint.localMatrix = MyMath::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
        if (joint.parent) { // 親がいれば親の行列を掛ける
            joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
        } else { // 親がいないのでlocalMatrixとskeletonSpaceMatrixは一致する
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

Skeleton CreateSkeleton(const Node& rootNode) {
    Skeleton skeleton;
    skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);

    // 名前とindexのマッピングを行いアクセスしやすくする
    for (const Joint& joint : skeleton.joints) {
        skeleton.jointMap.emplace(joint.name, joint.index);
    }

    Update(skeleton);

    return skeleton;
}

void Model::InitializeSphere(ModelCommon *modelCommon, int subdivision) {
	this->modelCommon_ = modelCommon;
	modelData.vertices = ModelPrimitiveBuilder::BuildSphereVertices(subdivision);
	CreateVertexData();
	CreateMaterialData();
	SetTextureFilePath("resources/white1x1.png");
	RecalculateBounds();
}

void Model::InitializePlane(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData.vertices = ModelPrimitiveBuilder::BuildPlaneVertices();
	CreateVertexData();
	CreateMaterialData();
	SetTextureFilePath("resources/white1x1.png");
	RecalculateBounds();
}

void Model::InitializeBox(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData.vertices = ModelPrimitiveBuilder::BuildBoxVertices();
	CreateVertexData();
	CreateMaterialData();
	SetTextureFilePath("resources/white1x1.png");
	RecalculateBounds();
}

void Model::InitializeRing(ModelCommon *modelCommon, int subdivision, float outerRadius, float innerRadius,
	bool isUvHorizontal, const Vector4& innerColor, const Vector4& outerColor,
	float startAngleDegree, float endAngleDegree, float fadeAngleDegree) {
	this->modelCommon_ = modelCommon;
	modelData.vertices = ModelPrimitiveBuilder::BuildRingVertices(
		subdivision, outerRadius, innerRadius, isUvHorizontal,
		innerColor, outerColor, startAngleDegree, endAngleDegree, fadeAngleDegree);
	CreateVertexData();
	CreateMaterialData();
	SetTextureFilePath("resources/gradationLine.png");
	RecalculateBounds();
}

void Model::InitializeCylinder(ModelCommon *modelCommon,
	int subdivision, int verticalSubdivision,
	float topRadiusX, float topRadiusZ,
	float bottomRadiusX, float bottomRadiusZ,
	float height,
	const Vector4& topColor, const Vector4& bottomColor,
	float startAngleDegree, float endAngleDegree,
	bool isUvFlipped) {
	this->modelCommon_ = modelCommon;
	modelData.vertices = ModelPrimitiveBuilder::BuildCylinderVertices(
		subdivision, verticalSubdivision, topRadiusX, topRadiusZ,
		bottomRadiusX, bottomRadiusZ, height, topColor, bottomColor,
		startAngleDegree, endAngleDegree, isUvFlipped);
	CreateVertexData();
	CreateMaterialData();
	if (materialData) {
		materialData->enableLighting = 0;
	}
	SetTextureFilePath("resources/gradationLine.png");
	RecalculateBounds();
}

void Model::InitializeCylinderShell(ModelCommon* modelCommon,
	int subdivision, int verticalSubdivision, float radius, float height, float thickness,
	float startAngleDegree, float endAngleDegree) {
	this->modelCommon_ = modelCommon;
	modelData = ModelData{};
	modelData.vertices = ModelPrimitiveBuilder::BuildCylinderShellVertices(
		subdivision, verticalSubdivision, radius, height, thickness,
		startAngleDegree, endAngleDegree);
	CreateVertexData();
	CreateMaterialData();
	if (materialData) {
		materialData->enableLighting = 0;
	}
	SetTextureFilePath("resources/white1x1.png");
	RecalculateBounds();
}

void Model::InitializeLine(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData = ModelData{};

	VertexData v1, v2;
	v1.position = { 0.0f, 0.0f, 0.0f, 1.0f };
	v1.normal = { 0.0f, 1.0f, 0.0f };
	v1.texcoord = { 0.0f, 0.0f };
	v1.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	v2.position = { 0.0f, 1.0f, 0.0f, 1.0f };
	v2.normal = { 0.0f, 1.0f, 0.0f };
	v2.texcoord = { 1.0f, 1.0f };
	v2.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	modelData.vertices.push_back(v1);
	modelData.vertices.push_back(v2);

	modelData.isLine = true;
	modelData.isStrip = false;
	modelData.isSkinned = false;
	inputVertexResource = nullptr;

	CreateVertexData();
	CreateMaterialData();

	if (materialData) {
		materialData->enableLighting = 0; // ラインはライティング不要
	}

	textureFilePath_ = "resources/white1x1.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
	RecalculateBounds();
}

void Model::UpdateLineVertices(const std::vector<VertexData>& lines) {
	if (lines.empty()) return;
	
	bool needRecreate = (modelData.vertices.size() != lines.size());
	modelData.vertices = lines;
	modelData.isLine = true;
	modelData.isStrip = false;
	modelData.isSkinned = false;
	inputVertexResource = nullptr;

	if (needRecreate || !vertexResource) {
		CreateVertexData();
	} else {
		// 既存のvertexResource(UPLOADヒープ)をマップして更新する
		if (!UploadVertexData(vertexResource.Get(), modelData.vertices)) {
			CreateVertexData();
		}
	}
	RecalculateBounds();
}

SkinCluster Model::CreateSkinCluster(const Skeleton& skeleton) {
	SkinCluster skinCluster;
	skinCluster.isValid = false;
	if (!modelData.isSkinned || modelData.boneNames.empty()) return skinCluster;

	// ボーンインデックスの最大値をチェックして、パレットサイズを確定する
	uint32_t maxBoneIndex = 0;
	for (const auto& vertex : modelData.vertices) {
		for (int i = 0; i < 4; ++i) {
			if (vertex.weight[i] > 0.0f) {
				maxBoneIndex = (std::max)(maxBoneIndex, (uint32_t)vertex.index[i]);
			}
		}
	}
	// パレットのサイズは、ボーン名リストの数と最大インデックスの大きい方にする
	uint32_t paletteSize = (std::max)((uint32_t)modelData.boneNames.size(), maxBoneIndex + 1);

	// パレットリソースの作成
	skinCluster.paletteResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(WellKnownPalette) * paletteSize);
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&skinCluster.mappedPalette));
	skinCluster.paletteAddress = skinCluster.paletteResource->GetGPUVirtualAddress();

	// スキニング情報定数バッファの作成
	skinCluster.skinningInfoResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(SkinningInformation));
	skinCluster.skinningInfoResource->Map(0, nullptr, reinterpret_cast<void**>(&skinCluster.mappedSkinningInfo));
	skinCluster.mappedSkinningInfo->numVertices = static_cast<uint32_t>(modelData.vertices.size());
	skinCluster.mappedSkinningInfo->numBones = paletteSize;

	skinCluster.inverseBindMatrices.resize(paletteSize);
	skinCluster.boneIndexToJointIndex.resize(paletteSize);

	for (size_t i = 0; i < modelData.boneNames.size(); ++i) {
		const std::string& jointName = modelData.boneNames[i];
		auto it = skeleton.jointMap.find(jointName);
		if (it != skeleton.jointMap.end()) {
			skinCluster.boneIndexToJointIndex[i] = it->second;
		} else {
			skinCluster.boneIndexToJointIndex[i] = -1;
		}
		
		auto itSkin = modelData.skinClusterData.find(jointName);
		if (itSkin != modelData.skinClusterData.end()) {
			skinCluster.inverseBindMatrices[i] = itSkin->second.inverseBindMatrix;
		} else {
			skinCluster.inverseBindMatrices[i] = math->MakeIdentity4x4();
		}
	}

	for (size_t i = 0; i < skinCluster.boneIndexToJointIndex.size(); ++i) {
		const int32_t jointIndex = skinCluster.boneIndexToJointIndex[i];
		if (jointIndex >= 0 && jointIndex < static_cast<int32_t>(skeleton.joints.size())) {
			Matrix4x4 paletteMatrix = math->Multiply(
				skinCluster.inverseBindMatrices[i],
				skeleton.joints[jointIndex].skeletonSpaceMatrix);

			if (std::isnan(paletteMatrix.m[0][0])) {
				paletteMatrix = math->MakeIdentity4x4();
			}
			skinCluster.mappedPalette[i].skeletonSpaceMatrix = paletteMatrix;
		} else {
			skinCluster.mappedPalette[i].skeletonSpaceMatrix = math->MakeIdentity4x4();
		}
	}

	// ビューの生成 (SrvManager への登録)
	SrvManager* srvManager = SrvManager::GetInstance();
	
	// 入力頂点SRV (t1)
	skinCluster.srvIndexInputVertex = srvManager->Allocate();
	srvManager->CreateSRVforStructuredBuffer(skinCluster.srvIndexInputVertex, inputVertexResource.Get(), static_cast<UINT>(modelData.vertices.size()), sizeof(VertexData));

	// 出力頂点UAV (u0)
	skinCluster.uavIndexOutputVertex = srvManager->Allocate();
	srvManager->CreateUAVforStructuredBuffer(skinCluster.uavIndexOutputVertex, vertexResource.Get(), static_cast<UINT>(modelData.vertices.size()), sizeof(VertexData));

	// パレットSRV (t0)
	skinCluster.srvIndexPalette = srvManager->Allocate();
	srvManager->CreateSRVforStructuredBuffer(skinCluster.srvIndexPalette, skinCluster.paletteResource.Get(), static_cast<UINT>(paletteSize), sizeof(WellKnownPalette));
	
	skinCluster.isValid = true;
	return skinCluster;
}

void Model::UpdateSkinCluster(SkinCluster& skinCluster, const Skeleton& skeleton) {
	if (!skinCluster.isValid || !skinCluster.mappedPalette) return;

	for (size_t i = 0; i < skinCluster.boneIndexToJointIndex.size(); ++i) {
		int32_t jointIndex = skinCluster.boneIndexToJointIndex[i];
		if (jointIndex >= 0 && jointIndex < static_cast<int32_t>(skeleton.joints.size())) {
			Matrix4x4 skeletonSpaceMatrix = skeleton.joints[jointIndex].skeletonSpaceMatrix;
			// InverseBind * SkeletonSpace
			Matrix4x4 paletteMatrix = math->Multiply(skinCluster.inverseBindMatrices[i], skeletonSpaceMatrix);
			
			// NaNチェック
			if (std::isnan(paletteMatrix.m[0][0])) {
				paletteMatrix = math->MakeIdentity4x4();
			}
			
			skinCluster.mappedPalette[i].skeletonSpaceMatrix = paletteMatrix;
		} else {
			skinCluster.mappedPalette[i].skeletonSpaceMatrix = math->MakeIdentity4x4();
		}
	}
	// 更新されたのでスキニングが必要
	skinCluster.isUpdated = false;
}

bool Model::Skinning(SkinCluster& skinCluster) {
	if (!skinCluster.isValid || !modelData.isSkinned) return false;
	// すでにこのフレームでスキニング済みならスキップ
	if (skinCluster.isUpdated) return false;

	uint32_t vertexCount = static_cast<uint32_t>(modelData.vertices.size());
	if (vertexCount == 0 || vertexCount > 1000000) return false; // 異常な頂点数はスキップ

	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();
	SrvManager* srvManager = SrvManager::GetInstance();

	// --- Compute Shader 実行前のバリア (Vertex -> UAV) ---
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = vertexResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; 
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	// パイプライン設定
	commandList->SetComputeRootSignature(modelCommon_->GetSkinningRootSignature());
	commandList->SetPipelineState(modelCommon_->GetSkinningPipelineState());

	// 記述子テーブルのセット
	srvManager->SetComputeRootDescriptorTable(0, skinCluster.srvIndexPalette);
	srvManager->SetComputeRootDescriptorTable(1, skinCluster.srvIndexInputVertex);
	srvManager->SetComputeRootDescriptorTable(2, skinCluster.uavIndexOutputVertex);
	commandList->SetComputeRootConstantBufferView(3, skinCluster.skinningInfoResource->GetGPUVirtualAddress());

	// ボーン数が0の場合はスキップ (skinningInfo はこの関数の冒頭で定義すべき)
	if (vertexCount == 0) {
		skinCluster.isUpdated = true;
		return true; 
	}

	// Dispatch (スレッド数を64に下げてより細かく実行)
	commandList->Dispatch((vertexCount + 63) / 64, 1, 1);

	// --- Compute Shader 実行後のバリア (UAV -> Vertex) ---
	// 1. 書き込み完了を待機 (UAV Barrier)
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = vertexResource.Get();
	commandList->ResourceBarrier(1, &uavBarrier);

	// 2. 状態を遷移 (Transition Barrier)
	D3D12_RESOURCE_BARRIER transBarrier = {};
	transBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transBarrier.Transition.pResource = vertexResource.Get();
	transBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	commandList->ResourceBarrier(1, &transBarrier);

	// 実行済みフラグを立てる
	skinCluster.isUpdated = true;
	return true;
}

void Model::InitializeTrail(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData = ModelData{};

	// 初期状態は空（またはダミー頂点）にしておく
	VertexData v{};
	modelData.vertices.push_back(v);
	modelData.vertices.push_back(v);
	modelData.isLine = false;
	modelData.isSkinned = false;
	inputVertexResource = nullptr;

	modelData.isStrip = true; // ストリップ描画を有効化

	CreateVertexData();
	CreateMaterialData();

	if (materialData) {
		materialData->enableLighting = 0; // 煙なのでライティング不要
	}

	// とりあえず既存の画像を割り当てておく（後で煙の画像に変えられます）
	textureFilePath_ = "resources/white1x1.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
	RecalculateBounds();
}

void Model::UpdateTrailVertices(const std::vector<VertexData> &vertices, bool isTriangleStrip) {
	if (vertices.empty()) return;

	// 頂点数が変わったらバッファを作り直す必要があるかチェック
	bool needRecreate = (modelData.vertices.size() != vertices.size());
	modelData.vertices = vertices;
	modelData.isLine = false;
	modelData.isStrip = isTriangleStrip;
	modelData.isSkinned = false;
	inputVertexResource = nullptr;

	if (needRecreate || !vertexResource) {
		CreateVertexData();
	} else {
		// 既存のvertexResource(UPLOADヒープ)をマップして更新する
		if (!UploadVertexData(vertexResource.Get(), modelData.vertices)) {
			CreateVertexData();
		}
	}
	RecalculateBounds();
}

void Model::RecalculateBounds() {
	if (modelData.vertices.empty()) {
		cachedBoundsCenter_ = { 0.0f, 0.0f, 0.0f };
		cachedHalfExtents_ = { 1.0f, 1.0f, 1.0f };
		return;
	}

	Vector3 minPos = { modelData.vertices[0].position.x, modelData.vertices[0].position.y, modelData.vertices[0].position.z };
	Vector3 maxPos = minPos;

	for (const auto& vertex : modelData.vertices) {
		minPos.x = (std::min)(minPos.x, vertex.position.x);
		minPos.y = (std::min)(minPos.y, vertex.position.y);
		minPos.z = (std::min)(minPos.z, vertex.position.z);

		maxPos.x = (std::max)(maxPos.x, vertex.position.x);
		maxPos.y = (std::max)(maxPos.y, vertex.position.y);
		maxPos.z = (std::max)(maxPos.z, vertex.position.z);
	}

	cachedBoundsCenter_ = {
		(minPos.x + maxPos.x) * 0.5f,
		(minPos.y + maxPos.y) * 0.5f,
		(minPos.z + maxPos.z) * 0.5f
	};

	cachedHalfExtents_ = {
		(maxPos.x - minPos.x) * 0.5f,
		(maxPos.y - minPos.y) * 0.5f,
		(maxPos.z - minPos.z) * 0.5f
	};
}

