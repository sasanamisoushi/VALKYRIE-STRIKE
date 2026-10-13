#include "PostEffect.h"
#include <cassert>
#include <externals/imgui/imgui.h>

PostEffect* PostEffect::instance_ = nullptr;

void PostEffect::Initialize() {
    instance_ = this;

    auto dxCommon = DirectXCommon::GetInstance();
    auto device = dxCommon->GetDevice();

    // 1. ルートシグネチャの作成 (テクスチャを1枚受け取るだけ)
    // [0] 色のテクスチャ (t0) 用のレンジ
    D3D12_DESCRIPTOR_RANGE rangeColor[1] = {};
    rangeColor[0].BaseShaderRegister = 0; // t0
    rangeColor[0].NumDescriptors = 1;
    rangeColor[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeColor[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // [1] 深度のテクスチャ (t1) 用のレンジ
    D3D12_DESCRIPTOR_RANGE rangeDepth[1] = {};
    rangeDepth[0].BaseShaderRegister = 1; // t1レジスタを使う
    rangeDepth[0].NumDescriptors = 1;
    rangeDepth[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeDepth[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // [2] ノイズのテクスチャ (t2)
    D3D12_DESCRIPTOR_RANGE rangeNoise[1] = {};
    rangeNoise[0].BaseShaderRegister = 2; // t2レジスタ
    rangeNoise[0].NumDescriptors = 1;
    rangeNoise[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeNoise[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ルートパラメーターの設定
    D3D12_ROOT_PARAMETER rootParams[4] = {};

    // 0番目：色のテクスチャ (t0)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[0].DescriptorTable.pDescriptorRanges = rangeColor;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;

    // 1番目：32ビット定数 (b0)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[1].Constants.ShaderRegister = 0;
    rootParams[1].Constants.Num32BitValues = 16;

    // 2番目：深度のテクスチャ (t1)
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[2].DescriptorTable.pDescriptorRanges = rangeDepth;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;

    // 3番目：ノイズのテクスチャ (t2)
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[3].DescriptorTable.pDescriptorRanges = rangeNoise;
    rootParams[3].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootSignatureDesc.pParameters = rootParams;
    rootSignatureDesc.NumParameters = 4;
    rootSignatureDesc.pStaticSamplers = &sampler;
    rootSignatureDesc.NumStaticSamplers = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    assert(SUCCEEDED(hr));
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    // 2. PSOの作成
    auto vsBlob = dxCommon->CompileShader(L"resources/shaders/CopyImage.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon->CompileShader(L"resources/shaders/CopyImage.PS.hlsl", L"ps_6_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // ブレンドとラスタライザ（カリングなし）
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    // ★重要：深度テストは不要（画面にベタ塗りするだけだから）
    psoDesc.DepthStencilState.DepthEnable = FALSE;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    // トポロジーは三角形
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void PostEffect::Update() {

    param_.time += 1.0f / 60.0f;

    if (isAutoPlay_) {
        // 1秒間に autoPlaySpeed_ の速度でしきい値を進める
        param_.dissolveThreshold += autoPlaySpeed_ * (1.0f / 60.0f);

        // 完全に消えきって少し待ったら、また0に戻してループさせる
        if (param_.dissolveThreshold > 1.2f) {
            param_.dissolveThreshold = -0.2f;
        }
    }
}

void PostEffect::Draw(D3D12_GPU_DESCRIPTOR_HANDLE srvHandle, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle, D3D12_GPU_DESCRIPTOR_HANDLE noise0Handle, D3D12_GPU_DESCRIPTOR_HANDLE noise1Handle) {
    auto commandList = DirectXCommon::GetInstance()->GetCommandList();

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // RenderTextureの画像（SRV）をセットする
    commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

    // 1番目：定数（effectType_）をGPUに直接セット！
    commandList->SetGraphicsRoot32BitConstants(1, 16, &param_, 0);

    // 2番目（インデックス2）に深度テクスチャをセット
    commandList->SetGraphicsRootDescriptorTable(2, depthSrvHandle);
    
    // 3番目：ノイズテクスチャをセット
    if (noiseIndex_ == 0) {
        commandList->SetGraphicsRootDescriptorTable(3, noise0Handle);
    } else {
        commandList->SetGraphicsRootDescriptorTable(3, noise1Handle);
    }

    // 魔法の三角形を描画（頂点バッファなしで、頂点3つを描画と指示するだけ）
    commandList->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::DrawImGui() {
#ifdef ENABLE_IMGUI
    ImGui::Begin("Post Effect Settings");

    // エフェクト選択
    const char *items[] = { "Normal", "Grayscale", "Invert", "Sepia",
        "Vignette", "3x3 Box Filter", "5x5 Box Filter", "Gaussian Blur", "Edge Detection", "Radial Blur", "Dissolve","Random Noise", "Vignette Smoothing", "Fold Wave (Glitch/Chroma)", "System Shutdown (CRT)" };
    ImGui::Combo("Type", &param_.effectType, items, IM_ARRAYSIZE(items));

    ImGui::Separator();

    // エフェクトごとの数値調整
    if (param_.effectType == 4 || param_.effectType == 12) { // Vignette
        ImGui::Text("Vignette Parameters");
        ImGui::SliderFloat("Radius", &param_.vignetteRadius, 0.0f, 1.0f);
        ImGui::SliderFloat("Softness", &param_.vignetteSoftness, 0.01f, 1.0f);
        if (param_.effectType == 12) {
            ImGui::Text("Smoothing Parameters");
            ImGui::SliderFloat("Intensity", &param_.blurIntensity, 0.0f, 10.0f);
        }
    } else if (param_.effectType >= 5 && param_.effectType <= 7 || param_.effectType == 9) { // Box Filter
        ImGui::Text("Blur Parameters");
        ImGui::SliderFloat("Intensity", &param_.blurIntensity, 0.0f, 10.0f);
    }
    else if (param_.effectType == 10) { // Dissolve
        ImGui::Text("Dissolve Parameters");
        // 自動再生のチェックボックスと速度設定
        ImGui::Checkbox("Auto Play", &isAutoPlay_);
        if (isAutoPlay_) {
            ImGui::SliderFloat("Play Speed", &autoPlaySpeed_, 0.1f, 2.0f);
        }

        // しきい値（0.0で完全に表示、1.0で完全に消える）
        ImGui::SliderFloat("Threshold", &param_.dissolveThreshold, 0.0f, 1.0f);
        // エッジの太さ（0.0でエッジなし、少し上げると燃えカスのようになる）
        ImGui::SliderFloat("Edge Width", &param_.dissolveEdgeWidth, 0.0f, 0.2f);

        // 光らせる色をカラーピッカーで変更！
        ImGui::ColorEdit3("Edge Color", param_.edgeColor);
        // 背景色のカラーピッカー
        ImGui::ColorEdit3("None Color", param_.noneColor);

        // ノイズ画像を切り替えるメニュー
        const char *noiseItems[] = { "noise0.png", "noise1.png" };
        ImGui::Combo("Noise Texture", &noiseIndex_, noiseItems, IM_ARRAYSIZE(noiseItems));
    }
    else if (param_.effectType == 14) { // System Shutdown (CRT)
        ImGui::Text("System Shutdown Parameters");
        ImGui::SliderFloat("Progress", &param_.dissolveThreshold, 0.0f, 1.0f);
        ImGui::SliderFloat("Glitch Strength", &param_.blurIntensity, 0.0f, 1.0f);
    }
    ImGui::End();
#endif
}
