#include "object3d.hlsli"

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    // U=0.5 is the ribbon center.  A broad Gaussian-like profile removes the
    // hard polygon edge without drawing a separate white-hot centre line.
    const float32_t centeredU = abs(input.texcoord.x * 2.0f - 1.0f);
    const float32_t softEdge = pow(saturate(1.0f - centeredU), 1.35f);

    // V is zero at the nozzle and one at the oldest sample.  Keep the root
    // almost opaque so it is physically connected to the plume; the plume
    // itself hides the tiny cap while the historical end fades away.
    const float32_t nozzleFade = smoothstep(-0.06f, 0.01f, input.texcoord.y);
    const float32_t tailFade = 1.0f - smoothstep(0.58f, 1.0f, input.texcoord.y);

    // A few narrow, spatially animated-looking filaments break up the flat
    // gradient.  They are tied to world space, so moving through them gives
    // the trail a flowing energy texture without a separate bitmap asset.
    const float32_t worldPhase = dot(input.worldPosition, float32_t3(3.1f, 5.3f, 4.7f));
    const float32_t threadPhase = input.texcoord.y * 58.0f + input.texcoord.x * 13.0f - worldPhase;
    const float32_t threads = 0.72f + 0.28f * pow(abs(sin(threadPhase)), 7.0f);
    const float32_t density = softEdge * nozzleFade * tailFade * threads;
    const float32_t alpha = input.color.a * density;
    if (alpha < 0.0001f) discard;

    PixelShaderOutput output;
    // Keep the emission as a soft colored halo.  The previous white core is
    // intentionally omitted so the trail stays light and does not dominate
    // the scene while turning.
    output.color = float32_t4(input.color.rgb * (0.52f + softEdge * 0.65f), alpha);
    return output;
}
