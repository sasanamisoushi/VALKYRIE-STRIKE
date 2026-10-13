#pragma once
#include "engine/Graphics/DirectXCommon.h"
#include <wrl.h>

class PostEffect {
public:
    static PostEffect* GetInstance() { return instance_; }

    int GetEffectType() const { return param_.effectType; }
    void SetEffectType(int type) { param_.effectType = type; }
    void SetVignette(float radius, float softness) {
        param_.effectType = 4;
        param_.vignetteRadius = radius;
        param_.vignetteSoftness = softness;
    }
    void SetVignetteSmoothing(float radius, float softness, float blurIntensity) {
        param_.effectType = 12;
        param_.vignetteRadius = radius;
        param_.vignetteSoftness = softness;
        param_.blurIntensity = blurIntensity;
    }
    // 画面中心へ向かうサンプリングで、画面外へ引き伸ばされるように見せる放射状ブラー。
    void SetRadialBlur(float intensity) {
        param_.effectType = 9;
        param_.blurIntensity = intensity;
    }
    // 通信切断・システムシャットダウン（ブラウン管テレビ電源OFFとグリッチ）
    void SetSystemShutdown(float progress, float glitchIntensity) {
        param_.effectType = 14;
        param_.dissolveThreshold = progress;
        param_.blurIntensity = glitchIntensity;
    }

    // パラメータをまとめた構造体（HLSLと並びを合わせる）
    struct PostEffectParam {
        int effectType;
        float vignetteRadius;
        float vignetteSoftness;
        float blurIntensity;
        float dissolveThreshold;
        float dissolveEdgeWidth;
        float pad1;
        float pad2;
        float edgeColor[3];
        float pad3;
        float noneColor[3]; 
        float time;

    };

    void Initialize();

    void Update();

    // 描画には、RenderTextureの画像(SRV)のGPUハンドルを渡す
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE srvHandle, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle, D3D12_GPU_DESCRIPTOR_HANDLE noise0Handle, D3D12_GPU_DESCRIPTOR_HANDLE noise1Handle);

    // ImGuiを描画する関数を追加
    void DrawImGui();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // 現在のエフェクトの種類を保持する変数 (0:通常, 1:モノクロ, 2:反転)
    int effectType_ = 0;

    // ImGuiでどちらのノイズを選ぶか保持する変数 (0: noise0, 1: noise1)
    int noiseIndex_ = 0;

    // 自動再生用の変数
    bool isAutoPlay_ = false;
    float autoPlaySpeed_ = 0.5f;

    // 初期値に色（オレンジ 1.0, 0.4, 0.3）を追加
    PostEffectParam param_ = {
        0, 0.3f, 0.5f, 1.5f,
        0.0f, 0.05f, 0.0f, 0.0f,
        1.0f, 0.4f, 0.3f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };

    static PostEffect* instance_;
};
