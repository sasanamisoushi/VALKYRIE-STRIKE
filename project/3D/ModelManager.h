#pragma once
#include <map>
#include <string>
#include <memory>
#include <3D/Model.h>
class ModelManager {
public:
	//シングルトンインスタンスの取得
	static ModelManager *GetInstance();
	//終了
	void Finalize();

	//初期化
	void Initialize(DirectXCommon *dxCommon);

	//モデルファイルの読み込み
	void LoadModel(const std::string &filePath);

	//モデルの検索
	Model *FindModel(const std::string &filePath);

	// 球モデルの作成
	void CreateSphereModel(const std::string &modelName, int subdivision);

	// 平面モデルの作成
	void CreatePlaneModel(const std::string &modelName);

	// ボックスモデルの作成
	void CreateBoxModel(const std::string &modelName);

	// リングモデルの作成
	void CreateRingModel(const std::string &modelName, int subdivision = 32, float outerRadius = 1.0f, float innerRadius = 0.5f,
		bool isUvHorizontal = true, const Vector4& innerColor = { 1.0f, 1.0f, 1.0f, 1.0f }, const Vector4& outerColor = { 1.0f, 1.0f, 1.0f, 1.0f },
		float startAngleDegree = 0.0f, float endAngleDegree = 360.0f, float fadeAngleDegree = 0.0f);

	// 円柱モデルの作成
	void CreateCylinderModel(const std::string &modelName, int subdivision = 32, float topRadius = 1.0f, float bottomRadius = 1.0f, float height = 3.0f);
	// 指定した角度範囲だけを持つ円柱側面。エネルギーシールドのような曲面に使う。
	void CreateCylinderArcModel(const std::string& modelName, int subdivision, float radius, float height,
		float startAngleDegree, float endAngleDegree);
	// 内外面と端面を持つ、実際に厚みのある円弧シールドモデル。
	void CreateCylinderArcShellModel(const std::string& modelName, int subdivision, float radius, float height,
		float thickness, float startAngleDegree, float endAngleDegree);

	// ラインモデルの作成
	void CreateLineModel(const std::string &modelName);

	// トレイルモデルの作成
	void CreateTrailModel(const std::string &modelName);

	ModelCommon *GetModelCommon() const { return modelCommon.get(); }

private:
	static ModelManager *instance;

	//コンストラクタ
	ModelManager() = default;

	//デストラクタ
	~ModelManager() = default;

	//コピーコンストラクタ
	ModelManager(const ModelManager &) = delete;

	//コピー代入演算子
	ModelManager &operator=(const ModelManager &) = delete;

	//モデルデータ
	std::map<std::string, std::unique_ptr<Model>>models;

	std::unique_ptr<ModelCommon> modelCommon;



};

