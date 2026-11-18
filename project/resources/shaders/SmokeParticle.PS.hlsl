#include "Particle.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

float32_t Hash21(float32_t2 p)
{
    p = frac(p * float32_t2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float32_t ValueNoise(float32_t2 p)
{
    float32_t2 i = floor(p);
    float32_t2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float32_t a = Hash21(i);
    float32_t b = Hash21(i + float32_t2(1.0f, 0.0f));
    float32_t c = Hash21(i + float32_t2(0.0f, 1.0f));
    float32_t d = Hash21(i + float32_t2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float32_t FbmNoise(float32_t2 p)
{
    float32_t value = 0.0f;
    float32_t amp = 0.55f;
    for (int i = 0; i < 3; ++i)
    {
        value += amp * ValueNoise(p);
        p *= 2.07f;
        amp *= 0.5f;
    }
    return value;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // テクスチャが白黒（アルファ=1）でも正しく外周が抜けるよう、輝度とアルファの両方を考慮
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float32_t texLuma = dot(textureColor.rgb, float32_t3(0.299f, 0.587f, 0.114f)) * textureColor.a;

    // UV中心からの距離で四角いポリゴン境界を完全に消す
    float32_t2 centered = input.texcoord - float32_t2(0.5f, 0.5f);
    float32_t dist = length(centered) * 2.0f;
    float32_t radialMask = saturate(1.0f - dist);
    radialMask = pow(radialMask, 1.65f);

    // 雲のようなモクモクした濃淡・輪郭のゆらぎを付与
    float32_t cloud = FbmNoise(input.texcoord * 5.5f + input.color.rg * 3.1f);
    float32_t smokeAlpha = radialMask * saturate(texLuma * 1.15f) * lerp(0.55f, 1.15f, cloud);
    smokeAlpha *= input.color.a;

    if (smokeAlpha <= 0.002f)
    {
        discard;
    }

    output.color = float32_t4(input.color.rgb, saturate(smokeAlpha));
    return output;
}
