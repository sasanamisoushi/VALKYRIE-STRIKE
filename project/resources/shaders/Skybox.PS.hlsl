#include "Skybox.hlsli"

// Texture2Dではなく、TextureCubeを使う
TextureCube<float4> skyboxTexture : register(t0);
SamplerState smp : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // キューブマップの雲模様は残しつつ、地上ステージに馴染むよう
    // 空頂部の青、地平線の湿った青灰色、地面近くの薄い霞へ補正する。
    const float3 direction = normalize(input.texcoord);
    const float height = saturate(direction.y * 0.5f + 0.5f);
    const float horizon = exp(-abs(direction.y) * 10.0f);
    const float skyBlend = smoothstep(0.18f, 0.88f, height);

    const float3 lowerHaze = float3(0.43f, 0.55f, 0.57f);
    const float3 horizonColor = float3(0.62f, 0.70f, 0.72f);
    const float3 upperSky = float3(0.19f, 0.41f, 0.63f);
    float3 atmosphere = lerp(lowerHaze, upperSky, skyBlend);
    atmosphere = lerp(atmosphere, horizonColor, horizon * 0.78f);

    // 元画像を明るく彩度の強いまま出すと、草地の色と分離して見える。
    // 輝度だけを主に引き継いで、雲の立体感は保持する。
    const float3 source = skyboxTexture.Sample(smp, direction).rgb;
    const float sourceLuminance = dot(source, float3(0.2126f, 0.7152f, 0.0722f));
    const float cloudDetail = saturate((sourceLuminance - 0.42f) * 1.35f);
    const float3 color = atmosphere * (0.72f + cloudDetail * 0.48f);
    return float4(color, 1.0f);
}
