#include "object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t alphaReference;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    // Emission is independent of sunlight and the reflective aircraft material.
    // Fade the silhouette of each rounded shell to soften the edge of the flame.
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    float32_t facing = saturate(abs(dot(normalize(input.normal), toEye)));
    // A hot core emits toward the viewer even when its long sides are edge-on.
    // Keep that density in axial/rear views while fading the outer glow softly.
    float32_t core = smoothstep(0.25f, 0.75f, input.color.g);
    float32_t minimumDensity = lerp(0.28f, 0.75f, core);
    float32_t edgeFade = lerp(minimumDensity, 1.0f, smoothstep(0.02f, 0.5f, facing));
    float32_t alpha = gMaterial.color.a * input.color.a * edgeFade;
    if (alpha < 0.0001f) discard;

    PixelShaderOutput output;
    output.color = float32_t4(gMaterial.color.rgb * input.color.rgb, alpha);
    return output;
}
