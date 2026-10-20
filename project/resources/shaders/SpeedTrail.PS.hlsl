#include "object3d.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    // U方向で中心を明るく、両端を透明にする。板ポリゴンの輪郭を見せず、
    // マクロスΔスクランブルのような幅を保った発光帯にする。
    const float32_t fromCenter = abs(input.texcoord.x * 2.0f - 1.0f);
    const float32_t edgeFade = pow(saturate(1.0f - fromCenter), 1.35f);
    // V座標はモデル側でスクロールされる。外炎には流動、白熱芯にはマッハリングを加える。
    const float32_t stream = 0.70f + 0.30f * sin(input.texcoord.y * 6.2831853f);
    const float32_t coreMask = smoothstep(0.24f, 0.62f, input.color.g);
    const float32_t machRing = pow(saturate(sin(input.texcoord.y * 6.2831853f)), 7.0f);
    const float32_t alpha = input.color.a * edgeFade * (0.72f + 0.28f * stream);
    if (alpha < 0.0001f) discard;

    PixelShaderOutput output;
    const float32_t3 energyColor = input.color.rgb * (0.52f + 0.72f * edgeFade) * stream;
    const float32_t3 machColor = float32_t3(1.0f, 0.88f, 1.0f) * (coreMask * machRing * 1.25f);
    output.color = float32_t4(energyColor + machColor, alpha);
    return output;
}
