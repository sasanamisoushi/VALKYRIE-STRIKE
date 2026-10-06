#include "Object3dCommon.h"
#include <externals/DirectXTex/d3dx12.h>
#include <cassert>

Object3dCommon *Object3dCommon::GetInstance() {
	static Object3dCommon instance;
	return &instance;
}

void Object3dCommon::Initialize(DirectXCommon *dxCommon) {
	//引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	//シェーダーの読み込み
	LoadShaders();

	//グラフィックスパイプラインの生成
	CreateGraphicsPipeline();
}

void Object3dCommon::SetCommonDrawSettings() {
	//ルートシグネチャをセットするコマンド
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	//グラフィックスパイプラインをセットするコマンド
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	//プリミティブポロジーをセットするコマンド
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetEffectDrawSettings() {
	//ルートシグネチャをセットするコマンド
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	//エフェクト用のグラフィックスパイプラインをセットするコマンド
	dxCommon_->GetCommandList()->SetPipelineState(effectPipelineState_.Get());
	//プリミティブポロジーをセットするコマンド
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetOverlayEffectDrawSettings() {
	// RootSignature と加算合成は通常のエフェクトと共有し、深度比較だけを無効化する。
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	dxCommon_->GetCommandList()->SetPipelineState(overlayEffectPipelineState_.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetThrusterDrawSettings() {
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	dxCommon_->GetCommandList()->SetPipelineState(thrusterPipelineState_.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetAlphaBlendDrawSettings() {
	//ルートシグネチャをセットするコマンド
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	//アルファブレンド用のグラフィックスパイプラインをセットするコマンド
	dxCommon_->GetCommandList()->SetPipelineState(alphaBlendPipelineState_.Get());
	//プリミティブトポロジーをセットするコマンド
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetLineDrawSettings() {
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	dxCommon_->GetCommandList()->SetPipelineState(linePipelineState_.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

void Object3dCommon::LoadShaders() {
	//Shaderをコンパイルする
	vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3D.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3D.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);
	thrusterPixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Thruster.PS.hlsl", L"ps_6_0");
	assert(thrusterPixelShaderBlob != nullptr);

}

void Object3dCommon::CreateRootSignature() {
	// ディスクリプタレンジ
	// t0: テクスチャ (SRV) 用
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// t1 環境マップ用のレンジ
	D3D12_DESCRIPTOR_RANGE descriptorRangeEnvMap[1] = {};
	descriptorRangeEnvMap[0].BaseShaderRegister = 1;
	descriptorRangeEnvMap[0].NumDescriptors = 1;
	descriptorRangeEnvMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeEnvMap[0].OffsetInDescriptorsFromTableStart= D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameter作成 
	D3D12_ROOT_PARAMETER rootParamerers[8] = {};

	// 0: マテリアルCBV (b0, Vertex/PixelShader)
	rootParamerers[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParamerers[0].Descriptor.ShaderRegister = 0;

	// 1: WVP/World行列CBV (b1, VertexShader)
	rootParamerers[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParamerers[1].Descriptor.ShaderRegister = 1;

	// 2: テクスチャDescriptorTable (t0, PixelShader)
	rootParamerers[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParamerers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParamerers[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// 3: 平行光源CBV (b1, PixelShader)
	rootParamerers[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[3].Descriptor.ShaderRegister = 1;

	// 4: カメラ座標CBV(b2,PixelShader)
	rootParamerers[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[4].Descriptor.ShaderRegister = 2;

	// 5: 環境マップDescriptorTable (t1, PixelShader)
	rootParamerers[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParamerers[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[5].DescriptorTable.pDescriptorRanges = descriptorRangeEnvMap;
	rootParamerers[5].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeEnvMap);

	// 6: 環境マップパラメータCBV (b3, PixelShader)
	rootParamerers[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[6].Descriptor.ShaderRegister = 3; // b3 に対応

	// 7: マトリックスパレットSRV (t0, VertexShader)
	rootParamerers[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParamerers[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParamerers[7].Descriptor.ShaderRegister = 0; // t0 in VS

	descriptionRootSignature.pParameters = rootParamerers;
	descriptionRootSignature.NumParameters = _countof(rootParamerers);


	// スタティックサンプラーの定義
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズと生成
	ID3DBlob *signatureBlob = nullptr;
	ID3DBlob *errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		//logger.Log( reinterpret_cast<char *>(errorBlob->GetBufferPointer())); // loggerがない場合はassertで代用
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateGraphicsPipeline() {

	// シェーダーロードとルートシグネチャ作成を前提
	CreateRootSignature();

	// InputLayoutの作成 (SpriteCommonと同じ内容でOK)
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"INDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの作成 (アルファブレンドは無効/デフォルト)
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerStateの作成
	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	// 3Dでは裏面を非表示にする (背面カリング) を有効にする
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みを許可
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual (手前にあるものを描画)
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	// ステンシルは今回は使用しないためデフォルト

	// PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;

	// シェーダーはObject3D用にロードされたものを使用 (LoadShaders()を別途実装しているはず)
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };

	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	pipelineState_ = nullptr;
	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr) && "パイプラインの作成に失敗しました！");

	// エフェクト用パイプラインの生成 (深度書き込み無効)
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = false;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	linePipelineState_ = nullptr;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&linePipelineState_));
	assert(SUCCEEDED(hr) && "line pipeline create failed");

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = true;
	graphicsPipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	
	effectPipelineState_ = nullptr;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&effectPipelineState_));
	assert(SUCCEEDED(hr) && "エフェクト用パイプラインの作成に失敗しました。");

	// 閉じた立体の外炎／白熱芯を発光として描く。裏面の二重加算を抑える。
	graphicsPipelineStateDesc.PS = { thrusterPixelShaderBlob->GetBufferPointer(), thrusterPixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	// Game View の ImGui 画像合成でも背景を透かさないよう、描画先のαを保つ。
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&thrusterPipelineState_));
	assert(SUCCEEDED(hr) && "スラスター用パイプラインの作成に失敗しました。");
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	// 機体のノズル光のように、カメラ角度によらず常に認識させたい発光用。
	// 深度値は参照も書き込みもしないので、機体・地形に隠れない。
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = false;
	overlayEffectPipelineState_ = nullptr;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&overlayEffectPipelineState_));
	assert(SUCCEEDED(hr) && "常時発光用パイプラインの作成に失敗しました。");
	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = true;

	// アルファブレンド（半透明）用
	graphicsPipelineStateDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	
	alphaBlendPipelineState_ = nullptr;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&alphaBlendPipelineState_));
	assert(SUCCEEDED(hr) && "アルファブレンド用パイプラインの作成に失敗しました。");
}
