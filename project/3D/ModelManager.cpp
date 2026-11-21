#include "ModelManager.h"

ModelManager *ModelManager::instance = nullptr;

ModelManager *ModelManager::GetInstance() {
	if (instance == nullptr) {
		instance = new ModelManager;
	}
	return instance;
}

void ModelManager::Finalize() {
	
	delete instance;
	instance = nullptr;
}

void ModelManager::Initialize(DirectXCommon *dxCommon) {

	modelCommon = std::make_unique<ModelCommon>();
	modelCommon->Initialize(dxCommon);
}

void ModelManager::LoadModel(const std::string &filePath) {

	//読み込み済みモデルを検索
	if (models.contains(filePath)) {
		//読み込み済みなら早期return
		return;
	}

	//モデルの生成とファイル読み込み,初期化
	std::unique_ptr<Model>model = std::make_unique<Model>();
	model->Initialize(modelCommon.get(), "resources", filePath);

	//モデルをmapコンテナに格納する
	models.insert(std::make_pair(filePath, std::move(model)));


}

Model *ModelManager::FindModel(const std::string &filePath) {

	//読み込み済みモデルを検索
	if (models.contains(filePath)) {
		//読み込みモデルを戻り値としてreturn
		return models.at(filePath).get();
	}

	//ファイル名一致なし
	return nullptr;
}

void ModelManager::CreateSphereModel(const std::string &modelName, int subdivision) {
	// 重複読み込み防止すでに同じ名前で登録されていたら何もしない
	if (models.contains(modelName)) {
		return;
	}

	// モデル生成
	std::unique_ptr<Model> newModel = std::make_unique<Model>();

	// モデル初期化
	newModel->InitializeSphere(modelCommon.get(), subdivision);

	// マップに登録
	models.insert(std::make_pair(modelName, std::move(newModel)));
}

void ModelManager::CreatePlaneModel(const std::string &modelName) {
	if (models.contains(modelName)) return;

	std::unique_ptr<Model> newModel = std::make_unique<Model>();
	newModel->InitializePlane(modelCommon.get());
	models.insert(std::make_pair(modelName, std::move(newModel)));
}

void ModelManager::CreateBoxModel(const std::string &modelName) {
	if (models.contains(modelName)) return;

	std::unique_ptr<Model> newModel = std::make_unique<Model>();
	newModel->InitializeBox(modelCommon.get());
	models.insert(std::make_pair(modelName, std::move(newModel)));
}

void ModelManager::CreateRingModel(const std::string &modelName, int subdivision, float outerRadius, float innerRadius,
	bool isUvHorizontal, const Vector4& innerColor, const Vector4& outerColor,
	float startAngleDegree, float endAngleDegree, float fadeAngleDegree) {
	if (models.contains(modelName)) return;

	std::unique_ptr<Model> newModel = std::make_unique<Model>();
	newModel->InitializeRing(modelCommon.get(), subdivision, outerRadius, innerRadius, isUvHorizontal, innerColor, outerColor, startAngleDegree, endAngleDegree, fadeAngleDegree);
	models.insert(std::make_pair(modelName, std::move(newModel)));
}

void ModelManager::CreateCylinderModel(const std::string &modelName, int subdivision, float topRadius, float bottomRadius, float height) {
	if (models.contains(modelName)) return;

	std::unique_ptr<Model> newModel = std::make_unique<Model>();
	newModel->InitializeCylinder(
		modelCommon.get(), 
		subdivision, 
		1, // verticalSubdivision
		topRadius, topRadius, // topRadiusX, topRadiusZ
		bottomRadius, bottomRadius, // bottomRadiusX, bottomRadiusZ
		height
	);
	models.insert(std::make_pair(modelName, std::move(newModel)));
}

void ModelManager::CreateCylinderArcModel(const std::string& modelName, int subdivision, float radius, float height,
	float startAngleDegree, float endAngleDegree) {
	if (models.contains(modelName)) return;

	std::unique_ptr<Model> newModel = std::make_unique<Model>();
	newModel->InitializeCylinder(modelCommon.get(), subdivision, 6,
		radius, radius, radius, radius, height,
		{ 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },
		startAngleDegree, endAngleDegree);
	models.insert(std::make_pair(modelName, std::move(newModel)));
}

void ModelManager::CreateCylinderArcShellModel(const std::string& modelName, int subdivision, float radius, float height,
	float thickness, float startAngleDegree, float endAngleDegree) {
	if (models.contains(modelName)) return;

	std::unique_ptr<Model> newModel = std::make_unique<Model>();
	newModel->InitializeCylinderShell(modelCommon.get(), subdivision, 6, radius, height, thickness,
		startAngleDegree, endAngleDegree);
	models.insert(std::make_pair(modelName, std::move(newModel)));
}

void ModelManager::CreateLineModel(const std::string &modelName) {
	if (models.contains(modelName)) return;

	std::unique_ptr<Model> newModel = std::make_unique<Model>();
	newModel->InitializeLine(modelCommon.get());
	models.insert(std::make_pair(modelName, std::move(newModel)));
}

void ModelManager::CreateTrailModel(const std::string &modelName) {
	if (models.contains(modelName)) return;

	std::unique_ptr<Model> newModel = std::make_unique<Model>();
	newModel->InitializeTrail(modelCommon.get());
	models.insert(std::make_pair(modelName, std::move(newModel)));
}
