#pragma once
#include "Model.h"
#include <string>

struct aiNode;

class ModelFileLoader {
public:
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadGltfFile(const std::string& directoryPath, const std::string& filename);
	static Node ReadNode(aiNode* node);
	static Node LoadNodeHierarchy(const std::string& directoryPath, const std::string& filename);
};
