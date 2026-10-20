#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cassert>
#include <MyMath.h>
#include"Model.h"
#include "ModelManager.h"
#include "engine/Camera/Camera.h"
#include <memory>

class Object3dCommon;

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose;
};

struct DirectionalLight {
	Vector4 color; //ライトの色
	Vector3 direction;
	float intensity;
};

struct CameraForGPU {
	Vector3 worldPosition;
};

// 環境マップの設定構造体（HLSLと合わせる）
struct EnvMapParam {
	int32_t enable;
	float weight;
	float padding[2];
	// ワールド空間のクリップ面。normal.xyz * worldPosition + distance が
	// 0 未満のピクセルを描画しない。演出用にオブジェクト単位で利用する。
	Vector4 clipPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
	int32_t clipPlaneEnabled = 0;
	float clipPlanePadding[3] = {};
};

class Object3d {
public: 

	//初期化
	void Initialize(Object3dCommon *object3dCommon);

	//更新
	void Update(Camera* renderCamera = nullptr);

	//描画
	void Draw();

	//座標変換行列データ作成
	void CreateTransformationData();

	//平行光源データ作成
	void CreateDirectionLightData();

	// カメラデータ作成関数
	void CreateCameraData();

	// データ作成関数
	void CreateEnvMapParamData();

	EulerTransform transform;
	//Transform cameraTransform;

	//セッター
	void SetModel(const std::string &filepath);
	void SetModelPointer(Model* modelPtr) { this->model = modelPtr; }
	void SetScale(const Vector3 &scale) { this->transform.scale = scale; }
	void SetRotate(const Vector3 &rotate) { this->transform.rotate = rotate; this->useQuaternion_ = false; }
	void SetTranslate(const Vector3 &translate) { this->transform.translate = translate; }
	void SetCamera(Camera *camera) { this->camera = camera; }
	void SetQuaternionRotate(const Quaternion& q) { this->quaternionRotate_ = q; this->useQuaternion_ = true; }
	void SetEnvironmentMapEnabled(bool enabled) {
		if (envMapParamData_) {
			envMapParamData_->enable = enabled ? 1 : 0;
		}
	}
	void SetWorldClipPlane(const Vector3& normal, float distance) {
		if (envMapParamData_) {
			envMapParamData_->clipPlane = { normal.x, normal.y, normal.z, distance };
			envMapParamData_->clipPlaneEnabled = 1;
		}
	}
	void ClearWorldClipPlane() {
		if (envMapParamData_) {
			envMapParamData_->clipPlaneEnabled = 0;
		}
	}

	//ゲッター
	const Vector3 &GetScale()const { return transform.scale; }
	const Vector3 &GetRotate()const { return transform.rotate; }
	const Vector3 &GetTranslate()const { return transform.translate; }
	const Quaternion& GetQuaternionRotate() const { return quaternionRotate_; }
	DirectionalLight *GetDirectionalLightData() const { return directionLightData; }
	Model* GetModel() const { return model; }

	SkinCluster skinCluster;

private:

	Object3dCommon *object3dCommon = nullptr;

	
	std::unique_ptr<MyMath> math = std::make_unique<MyMath>();

	//座標変換行列バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource;
	TransformationMatrix *transformationMatrixData = nullptr;

	//平行光源リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionLightResource;
	DirectionalLight *directionLightData = nullptr;

	

	bool useMonsterBall = true;


	//モデル
	Model *model = nullptr;

	//カメラ
	Camera *camera = nullptr;

	// カメラ用リソースとマップ用ポインタ
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource;
	CameraForGPU *cameraData = nullptr;
	

	// 環境マップパラメータ用のリソースとポインタ
	Microsoft::WRL::ComPtr<ID3D12Resource> envMapParamResource_;
	EnvMapParam *envMapParamData_ = nullptr;

	bool useQuaternion_ = false;
	Quaternion quaternionRotate_ = {0.0f, 0.0f, 0.0f, 1.0f};
};
