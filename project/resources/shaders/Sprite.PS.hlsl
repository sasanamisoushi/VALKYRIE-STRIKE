#include "Sprite.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
};
ConstantBuffer<Material> gMaterial : register(b0);

// C++側のRootSignatureの設定と合わせるために受け口(b1)を用意しておく
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // テクスチャのUV座標変換
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	// 回転でテクスチャ矩形の外へ出た場所を、サンプラーのWrapで反対側から
	// 引き戻さない。雲リングの四隅に矩形状の写り込みが出るのを防ぐ。
	if (transformedUV.x < 0.0f || transformedUV.x > 1.0f ||
		transformedUV.y < 0.0f || transformedUV.y > 1.0f)
	{
		discard;
	}
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // スプライトは2Dなので、光の計算はせずに「そのままの色」を画面に出す
    output.color = gMaterial.color * textureColor;
    
    return output;
}
