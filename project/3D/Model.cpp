#include "Model.h"
#include "Object3dCommon.h"
#include "ModelCommon.h"
#include "engine/Resource/TextureManager.h"
#include "engine/Graphics/SrvManager.h"
#include <algorithm>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <wrl.h>
#include <cmath>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "engine/base/Logger.h"

using Microsoft::WRL::ComPtr;

namespace {

std::wstring Utf8ToWide(const std::string& text) {
	if (text.empty()) {
		return {};
	}

	int wideSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, nullptr, 0);
	if (wideSize <= 0) {
		return std::wstring(text.begin(), text.end());
	}

	std::wstring wideText(static_cast<size_t>(wideSize), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), -1, wideText.data(), wideSize);
	if (!wideText.empty() && wideText.back() == L'\0') {
		wideText.pop_back();
	}
	return wideText;
}

std::ifstream OpenUtf8File(const std::string& filePath) {
	return std::ifstream(std::filesystem::path(Utf8ToWide(filePath)));
}

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
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = 1;
	materialData->uvTransform = math->MakeIdentity4x4();

	materialData->shininess = 40.0f;
	materialData->alphaReference = modelData.material.alphaReference;
}

MaterialData Model::LoadMaterialTemplateFile(const std::string &directoryPath, const std::string &filename) {
	//構築するMaterialData
	MaterialData materialData;
	//ファイルから読んだ1行を格納するもの
	std::string line;
	//ファイルを開く
	std::ifstream file = OpenUtf8File(directoryPath + "/" + filename);
	//とりあえず開けなかったら止める
	assert(file.is_open());

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			//連続してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

ModelData Model::LoadObjFile(const std::string &directoryPath, const std::string &filename) {
	ModelData modelData;
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals;  //法線
	std::vector<Vector2> texcoords; //テクスチャ座標
	std::string line;  //ファイルから読んだ1行を格納するもの
	std::map<std::string, uint32_t> vertexMap; // 頂点データの重複を防ぐためのマップ

	std::ifstream file = OpenUtf8File(directoryPath + "/" + filename);  //ファイルを開く
	assert(file.is_open()); //とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; //先頭の識別子を読む


		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f;
			normals.push_back(normal);
		} else if (identifier == "f") {
			// OBJの頂点定義は v/vt/vn だけでなく、v//vn や v の形式も許される。
			// 空のUV欄へ stoi を行うと例外でゲーム全体が停止するため、欠けた要素は既定値で補う。
			auto resolveIndex = [](const std::string &text, size_t count, size_t &outIndex) {
				if (text.empty() || count == 0) {
					return false;
				}
				try {
					const int index = std::stoi(text);
					if (index > 0 && static_cast<size_t>(index) <= count) {
						outIndex = static_cast<size_t>(index - 1);
						return true;
					}
					if (index < 0 && static_cast<size_t>(-index) <= count) {
						outIndex = count - static_cast<size_t>(-index);
						return true;
					}
				} catch (const std::exception &) {
				}
				return false;
			};

			auto getVertexIndex = [&](const std::string &vertexDefinition, uint32_t &outIndex) {
				auto existing = vertexMap.find(vertexDefinition);
				if (existing != vertexMap.end()) {
					outIndex = existing->second;
					return true;
				}

				std::string elements[3];
				std::istringstream definitionStream(vertexDefinition);
				for (std::string &element : elements) {
					std::getline(definitionStream, element, '/');
				}

				size_t positionIndex = 0;
				if (!resolveIndex(elements[0], positions.size(), positionIndex)) {
					return false;
				}

				Vector2 texcoord = { 0.0f, 0.0f };
				size_t texcoordIndex = 0;
				if (resolveIndex(elements[1], texcoords.size(), texcoordIndex)) {
					texcoord = texcoords[texcoordIndex];
				}

				Vector3 normal = { 0.0f, 1.0f, 0.0f };
				size_t normalIndex = 0;
				if (resolveIndex(elements[2], normals.size(), normalIndex)) {
					normal = normals[normalIndex];
				}

				VertexData vertex;
				vertex.position = positions[positionIndex];
				vertex.texcoord = { texcoord.x, texcoord.y, 0.0f, 0.0f };
				vertex.normal = { normal.x, normal.y, normal.z, 0.0f };
				vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
				outIndex = static_cast<uint32_t>(modelData.vertices.size());
				vertexMap[vertexDefinition] = outIndex;
				modelData.vertices.push_back(vertex);
				return true;
			};

			std::vector<std::string> faceVertices;
			for (std::string vertexDefinition; s >> vertexDefinition;) {
				faceVertices.push_back(vertexDefinition);
			}
			if (faceVertices.size() < 3) {
				continue;
			}

			// 三角形以外の面も扇形に分割する。
			for (size_t vertex = 2; vertex < faceVertices.size(); ++vertex) {
				uint32_t indices[3] = {};
				if (getVertexIndex(faceVertices[0], indices[0]) &&
					getVertexIndex(faceVertices[vertex - 1], indices[1]) &&
					getVertexIndex(faceVertices[vertex], indices[2])) {
					modelData.indices.insert(modelData.indices.end(), std::begin(indices), std::end(indices));
				}
			}
		} else if (identifier == "mtllib") {
			//matrialTemplateLidraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			//基本的にobjファイルと同一階層にmtlは存在されるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}
ModelData Model::LoadGltfFile(const std::string &directoryPath, const std::string &filename) {
	ModelData modelData;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_Triangulate | aiProcess_GenNormals);
	assert(scene && scene->HasMeshes());

	// メッシュインデックスからノード名へのマッピング用
	std::vector<std::string> meshNodeNames(scene->mNumMeshes);
	auto lambdaFindMeshNodes = [&](auto& self, aiNode* node) -> void {
		for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
			uint32_t mIdx = node->mMeshes[i];
			if (mIdx < meshNodeNames.size()) {
				meshNodeNames[mIdx] = node->mName.C_Str();
			}
		}
		for (uint32_t i = 0; i < node->mNumChildren; ++i) {
			self(self, node->mChildren[i]);
		}
	};
	lambdaFindMeshNodes(lambdaFindMeshNodes, scene->mRootNode);

	// 全ノード名の収集とノード名からボーンインデックスへの変換マップ
	std::vector<std::string> allNodeNames;
	auto lambdaCollectNodeNames = [&](auto& self, aiNode* node) -> void {
		allNodeNames.push_back(node->mName.C_Str());
		for (uint32_t i = 0; i < node->mNumChildren; ++i) {
			self(self, node->mChildren[i]);
		}
	};
	lambdaCollectNodeNames(lambdaCollectNodeNames, scene->mRootNode);

	std::map<std::string, uint32_t> nodeNameToBoneIndex;
	for (uint32_t i = 0; i < allNodeNames.size(); ++i) {
		nodeNameToBoneIndex[allNodeNames[i]] = i;
	}

	struct MeshRange {
		uint32_t baseVertex;
		uint32_t vertexCount;
	};
	std::vector<MeshRange> meshRanges(scene->mNumMeshes);

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		uint32_t baseVertex = (uint32_t)modelData.vertices.size();
		modelData.vertices.resize(baseVertex + mesh->mNumVertices);
		meshRanges[meshIndex] = { baseVertex, mesh->mNumVertices };

		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

			VertexData& vertex = modelData.vertices[baseVertex + vertexIndex];
			// 右手系->左手系への変換を忘れずに
			vertex.position = { -position.x, position.y, position.z, 1.0f };
			vertex.normal = { -normal.x, normal.y, normal.z, 0.0f };
			vertex.texcoord = { texcoord.x, texcoord.y, 0.0f, 0.0f };
			vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3);
			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				modelData.indices.push_back(baseVertex + vertexIndex);
			}
			// 面の向きを左手系にあわせるためインデックスの順番を入れ替える
			std::swap(modelData.indices[modelData.indices.size() - 2], modelData.indices[modelData.indices.size() - 1]);
		}

		if (mesh->HasBones()) {
			modelData.isSkinned = true;
			
			// ボーン名の重複なしリストを作成
			for (uint32_t i = 0; i < mesh->mNumBones; ++i) {
				std::string jointName = mesh->mBones[i]->mName.C_Str();
				if (std::find(modelData.boneNames.begin(), modelData.boneNames.end(), jointName) == modelData.boneNames.end()) {
					modelData.boneNames.push_back(jointName);
				}
			}

			// ボーン名 -> Index のマップを作成
			std::map<std::string, uint32_t> boneNameToIndex;
			for (uint32_t b = 0; b < modelData.boneNames.size(); ++b) {
				boneNameToIndex[modelData.boneNames[b]] = b;
			}

			for (uint32_t i = 0; i < mesh->mNumBones; ++i) {
				aiBone* bone = mesh->mBones[i];
				std::string jointName = bone->mName.C_Str();
				
				JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

				aiMatrix4x4 m = bone->mOffsetMatrix;
				Matrix4x4 invBind = {
					m.a1, m.b1, m.c1, m.d1,
					m.a2, m.b2, m.c2, m.d2,
					m.a3, m.b3, m.c3, m.d3,
					m.a4, m.b4, m.c4, m.d4
				};
				
				// x軸反転を適用 (Assimpが右手系のため左手系に合わせる)
				invBind.m[0][1] *= -1.0f;
				invBind.m[0][2] *= -1.0f;
				invBind.m[0][3] *= -1.0f;
				invBind.m[1][0] *= -1.0f;
				invBind.m[2][0] *= -1.0f;
				invBind.m[3][0] *= -1.0f;

				jointWeightData.inverseBindMatrix = invBind;

				uint32_t boneIndex = boneNameToIndex[jointName];

				for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
					uint32_t vId = baseVertex + bone->mWeights[weightIndex].mVertexId;
					float weight = bone->mWeights[weightIndex].mWeight;
					if (vId < modelData.vertices.size()) {
						for (int slot = 0; slot < 4; ++slot) {
							if (modelData.vertices[vId].weight[slot] == 0.0f) {
								modelData.vertices[vId].weight[slot] = weight;
								modelData.vertices[vId].index[slot] = (int32_t)boneIndex;
								break;
							}
						}
					}
				}
			}
		}

		if (scene->HasMaterials()) {
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
				aiString aiPath;
				material->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
				std::string texFile = aiPath.C_Str();
				std::string baseDir = filePath.substr(0, filePath.find_last_of('/'));
				modelData.material.textureFilePath = baseDir + "/" + texFile;
			}
		}
	}

	// 自然なスキンウェイトを持たないが、ノード階層とアニメーションが存在する場合にプログラム側でウェイトを自動設定する
	if (scene->HasAnimations() && !modelData.isSkinned && !allNodeNames.empty()) {
		modelData.isSkinned = true;
		modelData.boneNames = allNodeNames;

		modelData.rootNode = ReadNode(scene->mRootNode);
		std::map<std::string, Matrix4x4> nodeGlobalMatrices;
		auto lambdaComputeGlobalMatrices = [&](auto& self, const Node& node, const Matrix4x4& parentGlobal) -> void {
			Matrix4x4 globalMat = node.localMatrix * parentGlobal;
			nodeGlobalMatrices[node.name] = globalMat;
			for (const auto& child : node.children) {
				self(self, child, globalMat);
			}
		};
		Matrix4x4 identity = MyMath::MakeIdentity4x4();
		lambdaComputeGlobalMatrices(lambdaComputeGlobalMatrices, modelData.rootNode, identity);

		// Compute initial uncentered global bounds center
		Vector3 totalMin = { 1e10f, 1e10f, 1e10f };
		Vector3 totalMax = { -1e10f, -1e10f, -1e10f };
		bool hasVertices = false;

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
			std::string nodeName = meshNodeNames[meshIndex];
			Matrix4x4 globalMat = MyMath::MakeIdentity4x4();
			if (!nodeName.empty() && nodeGlobalMatrices.find(nodeName) != nodeGlobalMatrices.end()) {
				globalMat = nodeGlobalMatrices[nodeName];
			}
			const auto& range = meshRanges[meshIndex];
			for (uint32_t v = 0; v < range.vertexCount; ++v) {
				VertexData& vertex = modelData.vertices[range.baseVertex + v];
				Vector3 pos3 = { vertex.position.x, vertex.position.y, vertex.position.z };
				pos3 = MyMath::Transform(pos3, globalMat);

				totalMin.x = (std::min)(totalMin.x, pos3.x);
				totalMin.y = (std::min)(totalMin.y, pos3.y);
				totalMin.z = (std::min)(totalMin.z, pos3.z);

				totalMax.x = (std::max)(totalMax.x, pos3.x);
				totalMax.y = (std::max)(totalMax.y, pos3.y);
				totalMax.z = (std::max)(totalMax.z, pos3.z);
				hasVertices = true;
			}
		}

		Vector3 centerOffset = { 0.0f, 0.0f, 0.0f };
		if (hasVertices) {
			centerOffset = {
				(totalMin.x + totalMax.x) * 0.5f,
				(totalMin.y + totalMax.y) * 0.5f,
				(totalMin.z + totalMax.z) * 0.5f
			};
		}

		// Subtract centerOffset to center the model around origin (0, 0, 0)
		modelData.rootNode.transform.translate.x -= centerOffset.x;
		modelData.rootNode.transform.translate.y -= centerOffset.y;
		modelData.rootNode.transform.translate.z -= centerOffset.z;

		// Rebuild rootNode local matrix and clear/recompute global matrices
		modelData.rootNode.localMatrix = MyMath::MakeAffineMatrix(
			modelData.rootNode.transform.scale,
			modelData.rootNode.transform.rotate,
			modelData.rootNode.transform.translate
		);

		nodeGlobalMatrices.clear();
		lambdaComputeGlobalMatrices(lambdaComputeGlobalMatrices, modelData.rootNode, identity);

		for (const auto& name : allNodeNames) {
			JointWeightData jointWeightData;
			Matrix4x4 globalMat = MyMath::MakeIdentity4x4();
			if (nodeGlobalMatrices.find(name) != nodeGlobalMatrices.end()) {
				globalMat = nodeGlobalMatrices[name];
			}
			jointWeightData.inverseBindMatrix = MyMath::Inverse(globalMat);
			modelData.skinClusterData[name] = jointWeightData;
		}

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
			std::string nodeName = meshNodeNames[meshIndex];
			uint32_t boneIndex = 0;
			if (!nodeName.empty() && nodeNameToBoneIndex.find(nodeName) != nodeNameToBoneIndex.end()) {
				boneIndex = nodeNameToBoneIndex[nodeName];
			}

			Matrix4x4 globalMat = MyMath::MakeIdentity4x4();
			if (!nodeName.empty() && nodeGlobalMatrices.find(nodeName) != nodeGlobalMatrices.end()) {
				globalMat = nodeGlobalMatrices[nodeName];
			}

			const auto& range = meshRanges[meshIndex];
			for (uint32_t v = 0; v < range.vertexCount; ++v) {
				VertexData& vertex = modelData.vertices[range.baseVertex + v];
				
				// Transform position and normal to initial global space
				Vector3 pos3 = { vertex.position.x, vertex.position.y, vertex.position.z };
				pos3 = MyMath::Transform(pos3, globalMat);
				vertex.position = { pos3.x, pos3.y, pos3.z, 1.0f };

				Vector3 normal3 = { vertex.normal.x, vertex.normal.y, vertex.normal.z };
				Matrix4x4 normalMat = globalMat;
				normalMat.m[3][0] = 0.0f;
				normalMat.m[3][1] = 0.0f;
				normalMat.m[3][2] = 0.0f;
				normal3 = MyMath::Transform(normal3, normalMat);
				normal3 = MyMath::Normalize(normal3);
				vertex.normal = { normal3.x, normal3.y, normal3.z, 0.0f };

				vertex.index[0] = (int32_t)boneIndex;
				vertex.weight[0] = 1.0f;
				vertex.index[1] = 0; vertex.weight[1] = 0.0f;
				vertex.index[2] = 0; vertex.weight[2] = 0.0f;
				vertex.index[3] = 0; vertex.weight[3] = 0.0f;
			}
		}
	} else {
		// ルートノード情報を保存しておく
		modelData.rootNode = ReadNode(scene->mRootNode);
	}

	return modelData;
}

Node Model::ReadNode(aiNode* node) {
    Node result;
    aiVector3D scale, translate;
    aiQuaternion rotate;
    node->mTransformation.Decompose(scale, rotate, translate); // assimpの行列からSRTを抽出する関数を利用
    result.transform.scale = { scale.x, scale.y, scale.z }; // Scaleはそのまま
    result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w }; // x軸を反転、さらに回転方向が逆なので軸を反転させる・
    result.transform.translate = { -translate.x, translate.y, translate.z }; // x軸を反転
    
    result.localMatrix = MyMath::MakeAffineMatrix(result.transform.scale, result.transform.rotate, result.transform.translate);
    
    result.name = node->mName.C_Str();
    result.children.resize(node->mNumChildren);
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }
    return result;
}

Node Model::LoadNodeHierarchy(const std::string& directoryPath, const std::string& filename) {
    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;
    const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
    assert(scene && scene->mRootNode);
    return ReadNode(scene->mRootNode);
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
	// 引数で受け取ってメンバ変数に記録する
	this->modelCommon_ = modelCommon;

	// 球の頂点を計算するための変数
	const float pi = 3.1415926535f;
	const float latEvery = pi / subdivision;
	const float lonEvey = (2.0f * pi) / subdivision;

	std::vector<VertexData> tempVertices;

	// 球面上に頂点を計算
	for (int latIndex = 0; latIndex <= subdivision; ++latIndex) {
		float lat = -pi / 2.0f + latEvery * latIndex;
		for (int lonIndex = 0; lonIndex <= subdivision; ++lonIndex) {
			float lon = lonEvey * lonIndex;
			VertexData vertex{};

			// 法線と座標の計算
			vertex.normal.x = std::cos(lat) * std::cos(lon);
			vertex.normal.y = std::sin(lat);
			vertex.normal.z = std::cos(lat) * std::sin(lon);
			vertex.position = { vertex.normal.x,vertex.normal.y,vertex.normal.z,1.0f };

			// UV座標の計算
			vertex.texcoord = { float(lonIndex) / subdivision,1.0f - float(latIndex) / subdivision };

			// 頂点カラーのデフォルト
			vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };

			tempVertices.push_back(vertex);
		}
	}

	// 三角形リストとして　modelData.verticesに展開
	modelData.vertices.clear();
	for (int latIndex = 0; latIndex < subdivision; ++latIndex) {
		for (int lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
			int start = (latIndex * (subdivision + 1)) + lonIndex;

			int a = start;
			int b = start + 1;
			int c = start + (subdivision + 1);
			int d = start + (subdivision + 1) + 1;

			// 三角形1
			modelData.vertices.push_back(tempVertices[a]);
			modelData.vertices.push_back(tempVertices[c]);
			modelData.vertices.push_back(tempVertices[b]);

			// 三角形2
			modelData.vertices.push_back(tempVertices[b]);
			modelData.vertices.push_back(tempVertices[c]);
			modelData.vertices.push_back(tempVertices[d]);
		}
	}

	// 頂点データとマテリアルデータのバッファ作成
	CreateVertexData();
	CreateMaterialData();

	// テクスチャの設定（これが無いと透明・黒になって見えなくなる）
	textureFilePath_ = "resources/white1x1.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
	RecalculateBounds();
}

void Model::InitializePlane(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

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
	// 三角形1（左下、左上、右下）
	modelData.vertices.push_back(v[0]);
	modelData.vertices.push_back(v[1]);
	modelData.vertices.push_back(v[2]);
	// 三角形2（左上、右上、右下）
	modelData.vertices.push_back(v[1]);
	modelData.vertices.push_back(v[3]);
	modelData.vertices.push_back(v[2]);

	// バッファ作成
	CreateVertexData();
	CreateMaterialData();

	// テクスチャの設定
	textureFilePath_ = "resources/white1x1.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
	RecalculateBounds();
}

void Model::InitializeBox(ModelCommon *modelCommon) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();

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
		modelData.vertices.push_back({ p[i0], {uv00.x, uv00.y, 0.0f, 0.0f}, norm, c });
		modelData.vertices.push_back({ p[i1], {uv01.x, uv01.y, 0.0f, 0.0f}, norm, c });
		modelData.vertices.push_back({ p[i2], {uv10.x, uv10.y, 0.0f, 0.0f}, norm, c });

		modelData.vertices.push_back({ p[i1], {uv01.x, uv01.y, 0.0f, 0.0f}, norm, c });
		modelData.vertices.push_back({ p[i3], {uv11.x, uv11.y, 0.0f, 0.0f}, norm, c });
		modelData.vertices.push_back({ p[i2], {uv10.x, uv10.y, 0.0f, 0.0f}, norm, c });
		};

	// 前, 奥, 左, 右, 上, 下の6面を作る
	addFace(0, 1, 2, 3, nFront);
	addFace(6, 7, 4, 5, nBack);
	addFace(4, 5, 0, 1, nLeft);
	addFace(2, 3, 6, 7, nRight);
	addFace(1, 5, 3, 7, nTop);
	addFace(4, 0, 6, 2, nBottom);

	// バッファ作成
	CreateVertexData();
	CreateMaterialData();

	// テクスチャの設定
	textureFilePath_ = "resources/white1x1.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
	RecalculateBounds();
}

void Model::InitializeRing(ModelCommon *modelCommon, int subdivision, float outerRadius, float innerRadius,
	bool isUvHorizontal, const Vector4& innerColor, const Vector4& outerColor,
	float startAngleDegree, float endAngleDegree, float fadeAngleDegree) {
	this->modelCommon_ = modelCommon;
	modelData.vertices.clear();
	textureFilePath_ = "resources/white1x1.png";
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);

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
	for (int i = 0; i < subdivision; ++i) {
		int outer0 = 2 * i;
		int inner0 = 2 * i + 1;
		int outer1 = 2 * (i + 1);
		int inner1 = 2 * (i + 1) + 1;

		// 三角形1: outer0 -> inner0 -> inner1
		modelData.vertices.push_back(tempVertices[outer0]);
		modelData.vertices.push_back(tempVertices[inner0]);
		modelData.vertices.push_back(tempVertices[inner1]);

		// 三角形2: outer0 -> inner1 -> outer1
		modelData.vertices.push_back(tempVertices[outer0]);
		modelData.vertices.push_back(tempVertices[inner1]);
		modelData.vertices.push_back(tempVertices[outer1]);
	}

	// 頂点データとマテリアルデータのバッファ作成
	CreateVertexData();
	CreateMaterialData();

	// テクスチャの設定
	textureFilePath_ = "resources/gradationLine.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
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
	modelData.vertices.clear();
	textureFilePath_ = "resources/white1x1.png";
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);

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
	for (int v = 0; v < verticalSubdivision; ++v) {
		for (int h = 0; h < subdivision; ++h) {
			const VertexData& v0 = grid[v][h];
			const VertexData& v1 = grid[v + 1][h];
			const VertexData& v2 = grid[v][h + 1];
			const VertexData& v3 = grid[v + 1][h + 1];

			// 三角形1: v0 -> v1 -> v3
			modelData.vertices.push_back(v0);
			modelData.vertices.push_back(v3);
			modelData.vertices.push_back(v1);

			// 三角形2: v0 -> v3 -> v2
			modelData.vertices.push_back(v0);
			modelData.vertices.push_back(v2);
			modelData.vertices.push_back(v3);
		}
	}

	// 頂点データとマテリアルデータのバッファ作成
	CreateVertexData();
	CreateMaterialData();

	// デフォルトでライティングを無効化
	if (materialData) {
		materialData->enableLighting = 0;
	}

	// テクスチャの設定
	textureFilePath_ = "resources/gradationLine.png";
	modelData.material.textureFilePath = textureFilePath_;
	TextureManager::GetInstance()->LoadTexture(textureFilePath_);
	modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath_);
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

