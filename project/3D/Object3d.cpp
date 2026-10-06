#include "Object3d.h"
#include "Object3dCommon.h"
#include "engine/Resource/TextureManager.h"
#include <fstream>
#include <sstream>
#include <string>
#include <wrl.h>
#include <externals/imgui/imgui.h>

using Microsoft::WRL::ComPtr;

namespace {

constexpr bool kShowEnvMapDebugWindow = false;

template <typename T>
bool MapUploadBuffer(ID3D12Resource* resource, T*& mappedData) {
	mappedData = nullptr;
	if (!resource) {
		return false;
	}

	HRESULT hr = resource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
	return SUCCEEDED(hr) && mappedData != nullptr;
}

}

void Object3d::Initialize(Object3dCommon *object3dCommon) {
	//引数で受け取ってメンバ変数に記録する
	this->object3dCommon = object3dCommon;

	TextureManager::GetInstance()->LoadTexture("resources/SkyBox.dds");

	//モデルの読み込み
	this->SetModel("plane.obj");

	//座標変換行列データ作成
	CreateTransformationData();

	//平行光源データ作成
	CreateDirectionLightData();

	// カメラ用のデータの生成
	CreateCameraData();

	// データ作成関数
	CreateEnvMapParamData();

	transform={ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	//cameraTransform={ { 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,-10.0f } };

	//デフォルトカメラをセット
	this->camera = object3dCommon->GetDefaultCamera();
}

void Object3d::Update(Camera* renderCamera) {
	if (!object3dCommon || !transformationMatrixData) {
		return;
	}

	camera = renderCamera ? renderCamera : object3dCommon->GetDefaultCamera();

	//transform.rotate.y += 0.01f;

	// ImGuiで環境マップの設定ウィンドウを作る
#ifdef ENABLE_IMGUI 
	if (kShowEnvMapDebugWindow && envMapParamData_) {
	ImGui::Begin("Environment Map Settings");

	// オブジェクト自身のメモリアドレスをIDにして、ImGuiの混乱を防ぐ！
	ImGui::PushID(this);

	// bool変換用の一時変数
	bool isEnvMapEnabled = envMapParamData_->enable != 0;
	if (ImGui::Checkbox("Enable EnvMap", &isEnvMapEnabled)) {
		envMapParamData_->enable = isEnvMapEnabled ? 1 : 0;
	}

	// 反射率のスライダー (0.0 ～ 1.0)
	ImGui::SliderFloat("Reflectivity", &envMapParamData_->weight, 0.0f, 1.0f);

	// Pushしたら必ずPopする！
	ImGui::PopID();

	ImGui::End();
	}
#endif

	
	

	Matrix4x4 worldMatrix;
	if (useQuaternion_) {
		worldMatrix = math->MakeAffineMatrix(transform.scale, quaternionRotate_, transform.translate);
	} else {
		worldMatrix = math->MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	}

	//projectionMatrix
	Matrix4x4 projectionMatrix = math->MakePerspectiveFovMatrix(0.45f, float(WinApp::GetClientWidth()) / float(WinApp::GetClientHeight()), 0.1f, 100.0f);

	//worldViewProjectionMatrix
	Matrix4x4 worldViewProjectionMatrix;
	if (camera) {
		const Matrix4x4 &viewProjectionMatrix = camera->GetViewProjectionMatrix();
		worldViewProjectionMatrix = math->Multiply(worldMatrix, viewProjectionMatrix);
		if (cameraData) {
			cameraData->worldPosition = camera->GetTranslate();
		}
	} else {
		worldViewProjectionMatrix = worldMatrix;
	}
	
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;

	Matrix4x4 worldInverse = math->Inverse(worldMatrix);
	transformationMatrixData->WorldInverseTranspose = math->Transpose(worldInverse);
}

void Object3d::Draw() {

	if (model == nullptr) {
		return;
	}
	if (!object3dCommon || !wvpResource || !directionLightResource || !cameraResource || !envMapParamResource_) {
		return;
	}

	if (model->GetModelData().isLine) {
		object3dCommon->SetLineDrawSettings();
	}

	// スキニング実行
	if (skinCluster.isValid) {
		if (model->Skinning(skinCluster)) {
			// スキニング(ComputeShader)実行後にグラフィックスパイプラインを再セット
			object3dCommon->SetCommonDrawSettings();
		}
	}

	//TransformationMatrixCbufferの場所を設定
	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());


	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionLightResource->GetGPUVirtualAddress());

	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(
		4, cameraResource->GetGPUVirtualAddress());

	// 環境マップをインデックス5(t1)にセットする
	TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(object3dCommon->GetDxCommon()->GetCommandList(), 5, "resources/SkyBox.dds");

	object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(6, envMapParamResource_->GetGPUVirtualAddress());

	if (skinCluster.isValid && skinCluster.paletteResource) {
		object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootShaderResourceView(7, skinCluster.paletteAddress);
	} else {
		object3dCommon->GetDxCommon()->GetCommandList()->SetGraphicsRootShaderResourceView(7, wvpResource->GetGPUVirtualAddress());
	}

	//3Dモデルが割り当てられたら描画する
	if (model) {
		model->Draw();
	}
	
}



void Object3d::CreateTransformationData() {
	//Plane用のWVPリソース
	wvpResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	
	if (!MapUploadBuffer(wvpResource.Get(), transformationMatrixData)) {
		wvpResource.Reset();
		return;
	}
	transformationMatrixData->WVP = math->MakeIdentity4x4();
	transformationMatrixData->World = math->MakeIdentity4x4();
}

void Object3d::CreateDirectionLightData() {
	//平行光源用のリソース
	directionLightResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));
	
	//書き込むためのアドレスを取得
	if (!MapUploadBuffer(directionLightResource.Get(), directionLightData)) {
		directionLightResource.Reset();
		return;
	}

	directionLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionLightData->direction = { 1.0f,0.0f,0.0f };
	directionLightData->intensity = 1.0f;

	directionLightData->direction = math->Normalize(directionLightData->direction);

}

void Object3d::CreateCameraData() {
	// カメラ用のリソースを作成 (パディングを含めてアライメントに注意する必要がない程度のシンプルな構造体です)
	cameraResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(CameraForGPU));

	// 書き込むためのアドレスを取得
	if (!MapUploadBuffer(cameraResource.Get(), cameraData)) {
		cameraResource.Reset();
		return;
	}

	// 初期値
	cameraData->worldPosition = { 0.0f, 0.0f, 0.0f };


}

void Object3d::CreateEnvMapParamData() {

	// リソース生成 (CreateBufferResource内部で256バイトアライメントされている前提)
	envMapParamResource_ = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(EnvMapParam));
	// マップ
	if (!MapUploadBuffer(envMapParamResource_.Get(), envMapParamData_)) {
		envMapParamResource_.Reset();
		return;
	}
	// 初期値
	envMapParamData_->enable = 1;      // 最初はON
	envMapParamData_->weight = 0.3f;   // 30%の反射
}

void Object3d::SetModel(const std::string &filepath) {

	//モデルを検索してセットする
	model = ModelManager::GetInstance()->FindModel(filepath);
}
