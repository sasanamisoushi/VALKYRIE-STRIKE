#pragma once
#include "engine/Graphics/DirectXCommon.h"
#include "MyMath.h"



class SpriteCommon {
public:

	static SpriteCommon *GetInstance();

	//初期化
	void Initialize(DirectXCommon* dxCommon);

	DirectXCommon *GetDxCommon() { return dxCommon_; }

	//共通描画設定
	void SetCommonPipelineState();
	// 発光エフェクト用。描画済みの色へソース色を加算する。
	void SetAdditivePipelineState();

private:

	// コンストラクタを隠蔽・コピー禁止にする
	SpriteCommon() = default;
	~SpriteCommon() = default;
	SpriteCommon(const SpriteCommon &) = delete;
	SpriteCommon &operator=(const SpriteCommon &) = delete;

	//ピクセルシェーダーの読み込み
	void LoadShaders();

	//ルートシグナルチャ
	void CreateRootSignature();

	//グラフィックスパイプライン生成
	void CreateGraphicsPipeline();

	DirectXCommon *dxCommon_;


	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> additivePipelineState_;

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};

	// Shader blobs
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;

	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};

	//RasiterzerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
};
