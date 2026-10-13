#include "SpriteCommon.h"
#include "Logger.h"

SpriteCommon *SpriteCommon::GetInstance() {
	static SpriteCommon instance;
	return &instance;
}

void SpriteCommon::Initialize(DirectXCommon *dxCommon) {
	//引数で受け取ってメンバ変数に記録する
	dxCommon_ = dxCommon;

	LoadShaders();

	CreateGraphicsPipeline();
}

void SpriteCommon::SetCommonPipelineState() {
	//ルートシグネチャをセットするコマンド
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	
	//グラフィックスパイプラインをセットするコマンド
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());

	//プリミティブポロジーをセットするコマンド
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void SpriteCommon::SetAdditivePipelineState() {
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	dxCommon_->GetCommandList()->SetPipelineState(additivePipelineState_.Get());
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void SpriteCommon::LoadShaders() {
	//Shaderをコンパイルする
	vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Sprite.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Sprite.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);
}

void SpriteCommon::CreateRootSignature() {

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//RootParameter作成。複製設定できるので配列
	D3D12_ROOT_PARAMETER rootParamerers[4] = {};
	rootParamerers[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[0].Descriptor.ShaderRegister = 0;           //レジスタ番号0とバインド
	rootParamerers[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParamerers[1].Descriptor.ShaderRegister = 0;
	rootParamerers[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParamerers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParamerers[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
	rootParamerers[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParamerers[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParamerers[3].Descriptor.ShaderRegister = 1;
	descriptionRootSignature.pParameters = rootParamerers;//ルートパラメータ配列のポインタ
	descriptionRootSignature.NumParameters = _countof(rootParamerers);//配列の長さ


	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//シリアライズしてバイナルにする
	ID3DBlob *signatureBlob = nullptr;
	ID3DBlob *errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		logger.Log( reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	rootSignature_ = nullptr;
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

}

void SpriteCommon::CreateGraphicsPipeline() {

	CreateRootSignature();

	//InputLayoutの作成
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);
	
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	
	//裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	//RootSignature
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
	//InputLayout
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	//VertexShader
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };
	//PixelShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };
	//BlendState
	graphicsPipelineStateDesc.BlendState = blendDesc;
	//RasterizerState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;


	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//利用するトポロジのタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	//どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;

	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	//比較関数はLessEqual
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	//DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//実際に生成
	pipelineState_ = nullptr;
	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));

	// 透明度を係数にしつつ、既に描かれた画面の色へ発光色を足すパイプライン。
	// 通常UI用のアルファ合成とは分け、呼び出し側が必要なときだけ選べるようにする。
	D3D12_GRAPHICS_PIPELINE_STATE_DESC additivePipelineDesc = graphicsPipelineStateDesc;
	auto& additiveBlend = additivePipelineDesc.BlendState.RenderTarget[0];
	additiveBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	additiveBlend.DestBlend = D3D12_BLEND_ONE;
	additiveBlend.BlendOp = D3D12_BLEND_OP_ADD;
	additiveBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
	additiveBlend.DestBlendAlpha = D3D12_BLEND_ONE;
	additiveBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	additivePipelineState_ = nullptr;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&additivePipelineDesc,
		IID_PPV_ARGS(&additivePipelineState_));
	assert(SUCCEEDED(hr));
}
