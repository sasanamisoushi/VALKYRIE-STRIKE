#pragma once
#include <engine/Graphics/DirectXCommon.h>
#include "engine/Camera/Camera.h"

class Object3dCommon {
public:

	// シングルトンインスタンスの取得
	static Object3dCommon *GetInstance();

	//初期化
	void Initialize(DirectXCommon* dxCommon);

	//共通描画設定
	void SetCommonDrawSettings();

	//エフェクト用描画設定
	void SetEffectDrawSettings();

	// 常時見せたい発光用。深度テストを行わず、機体や地形の裏でも光を失わない。
	void SetOverlayEffectDrawSettings();
	// スラスター用。加算合成＋深度テストあり・深度書き込みなし＋背面カリング。
	void SetThrusterDrawSettings();
	
	//アルファブレンド（半透明）用描画設定
	void SetAlphaBlendDrawSettings();
	
	void SetLineDrawSettings();

	//シェーダーの読み込み
	void LoadShaders();

	DirectXCommon *GetDxCommon()const { return dxCommon_; }

	//setter
	void SetDefaultCamera(Camera *camera) { this->defaultCamera = camera; }

	//getter
	Camera *GetDefaultCamera()const { return defaultCamera; }

private:

	//コンストラクタを隠蔽・コピー禁止にする
	Object3dCommon() = default;
	~Object3dCommon() = default;
	Object3dCommon(const Object3dCommon &) = delete;
	Object3dCommon &operator=(const Object3dCommon &) = delete;

	//ルートシグネチャの作成
	void CreateRootSignature();

	//グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();


	DirectXCommon *dxCommon_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> effectPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> overlayEffectPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> thrusterPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> alphaBlendPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> linePipelineState_;

	// Shader blobs
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> thrusterPixelShaderBlob;

	Camera *defaultCamera = nullptr;
};

