#include "ModelFileLoader.h"
#include <algorithm>
#include <cassert>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

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

struct ObjLoadOptions {
	std::set<int> includedGroups;
	std::set<int> excludedGroups;
	Vector3 pivot = { 0.0f, 0.0f, 0.0f };
};

void ParseGroupList(const std::string& text, std::set<int>& destination) {
	std::stringstream list(text);
	for (std::string token; std::getline(list, token, ',');) {
		try {
			const size_t dash = token.find('-');
			const int first = std::stoi(token.substr(0, dash));
			const int last = dash == std::string::npos ? first : std::stoi(token.substr(dash + 1));
			for (int group = (std::min)(first, last); group <= (std::max)(first, last); ++group) {
				destination.insert(group);
			}
		} catch (const std::exception&) {
			// 不正な追加指定は無視し、OBJ本体の読み込みを継続する。
		}
	}
}

ObjLoadOptions ParseObjLoadOptions(const std::string& optionText) {
	ObjLoadOptions options;
	std::stringstream arguments(optionText);
	for (std::string argument; std::getline(arguments, argument, ';');) {
		const size_t equal = argument.find('=');
		if (equal == std::string::npos) continue;

		const std::string key = argument.substr(0, equal);
		const std::string value = argument.substr(equal + 1);
		if (key == "groups") {
			ParseGroupList(value, options.includedGroups);
		} else if (key == "exclude") {
			ParseGroupList(value, options.excludedGroups);
		} else if (key == "pivot") {
			std::stringstream values(value);
			std::string component;
			try {
				if (std::getline(values, component, ',')) options.pivot.x = std::stof(component);
				if (std::getline(values, component, ',')) options.pivot.y = std::stof(component);
				if (std::getline(values, component, ',')) options.pivot.z = std::stof(component);
			} catch (const std::exception&) {
				options.pivot = { 0.0f, 0.0f, 0.0f };
			}
		}
	}
	return options;
}

}

MaterialData ModelFileLoader::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
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
		} else if (identifier == "Kd") {
			// テクスチャを持たないOBJでも、MTLの拡散色をそのまま使えるようにする。
			s >> materialData.color.x >> materialData.color.y >> materialData.color.z;
		}
	}
	return materialData;
}

ModelData ModelFileLoader::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;
	const size_t optionStart = filename.find('#');
	const std::string sourceFilename = filename.substr(0, optionStart);
	const ObjLoadOptions options = optionStart == std::string::npos
		? ObjLoadOptions{}
		: ParseObjLoadOptions(filename.substr(optionStart + 1));
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals;  //法線
	std::vector<Vector2> texcoords; //テクスチャ座標
	std::string line;  //ファイルから読んだ1行を格納するもの
	std::map<std::string, uint32_t> vertexMap; // 頂点データの重複を防ぐためのマップ

	std::ifstream file = OpenUtf8File(directoryPath + "/" + sourceFilename);  //ファイルを開く
	assert(file.is_open()); //とりあえず開けなかったら止める
	int objectGroupIndex = -1;
	bool loadActiveGroup = options.includedGroups.empty();

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
		} else if (identifier == "o") {
			++objectGroupIndex;
			loadActiveGroup = (options.includedGroups.empty() || options.includedGroups.contains(objectGroupIndex)) &&
				!options.excludedGroups.contains(objectGroupIndex);
		} else if (identifier == "f") {
			if (!loadActiveGroup) {
				continue;
			}
			// OBJの頂点定義は v/vt/vn だけでなく、v//vn や v の形式も許される。
			// 空のUV欄へ stoi を行うと例外でゲーム全体が停止するため、欠けた要素は既定値で補う。
			auto resolveIndex = [](const std::string& text, size_t count, size_t& outIndex) {
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
				} catch (const std::exception&) {
				}
				return false;
			};

			auto getVertexIndex = [&](const std::string& vertexDefinition, uint32_t& outIndex) {
				auto existing = vertexMap.find(vertexDefinition);
				if (existing != vertexMap.end()) {
					outIndex = existing->second;
					return true;
				}

				std::string elements[3];
				std::istringstream definitionStream(vertexDefinition);
				for (std::string& element : elements) {
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

	// OBJの部位を別Object3dとして描画する場合は、指定された支点を原点にする。
	// 元ファイル自体は変更しないため、部位ごとの独立回転でも形状を崩さない。
	for (VertexData& vertex : modelData.vertices) {
		vertex.position.x -= options.pivot.x;
		vertex.position.y -= options.pivot.y;
		vertex.position.z -= options.pivot.z;
	}
	return modelData;
}

ModelData ModelFileLoader::LoadGltfFile(const std::string& directoryPath, const std::string& filename) {
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

Node ModelFileLoader::ReadNode(aiNode* node) {
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

Node ModelFileLoader::LoadNodeHierarchy(const std::string& directoryPath, const std::string& filename) {
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
	assert(scene && scene->mRootNode);
	return ReadNode(scene->mRootNode);
}
