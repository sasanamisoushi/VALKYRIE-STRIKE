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
Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentMap : register(t1); // 環境マップ用のキューブテクスチャ (t1レジスタに割り当て)
SamplerState gSampler : register(s0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};


ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

// 環境マップ用の設定パラメータ (b3レジスタ)
struct EnvMapParam
{
    int32_t enable; // 0:OFF, 1:ON
    float32_t weight; // 反射の強さ
};
ConstantBuffer<EnvMapParam> gEnvMapParam : register(b3);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

    if (gMaterial.enableLighting != 0)
    {
    
    // 1. 光源へ向かうベクトルを求める (directionは光の進む向きなのでマイナスをかける)
        float32_t3 lightDirection = normalize(-gDirectionalLight.direction);

    // 2. 視線(toEye)と光(lightDirection)のハーフベクトルを求める
    float32_t3 halfVector = normalize(lightDirection + toEye);

    // 3. 法線とハーフベクトルの内積を求める
    float NDotH = dot(normalize(input.normal), halfVector);

    // 4. ハイライトの強さを計算
    float specularPow = pow(saturate(NDotH), gMaterial.shininess);
   
    float Ndotl = dot(normalize(input.normal), -gDirectionalLight.direction);
    float cos = pow(Ndotl * 0.5f + 0.5f, 2.0f);
        
    float3 diffuse = gMaterial.color.rgb * textureColor.rgb * input.color.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        
    // shininess が 0 以下のモデルは、スペキュラーハイライトを使わない。
    float3 specular = gMaterial.shininess > 0.0f
        ? gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow
        : float3(0.0f, 0.0f, 0.0f);
        
    output.color.rgb = diffuse + specular;
        if (gEnvMapParam.enable != 0) // ONのときだけ実行
        {
        // 入射ベクトル
            float3 incident = -toEye;
        
        // 反射ベクトルを計算(入射ベクトルと法線を使用)

            float3 refVector = reflect(incident, normalize(input.normal));
        
        // キューブマップから反射した方向の色をサンプリング
            float4 envColor = gEnvironmentMap.Sample(gSampler, refVector);
        
        // 元の色（ディフューズ＋スペキュラー）と、環境マップの色を合成
        // lerpの第3引数(0.3f)は「反射の強さ(30%映り込む)」です。金属っぽくするなら0.8等に上げます。
            output.color.rgb = output.color.rgb + (envColor.rgb * gEnvMapParam.weight);
        }
    }
    else
    {
        output.color.rgb = gMaterial.color.rgb * textureColor.rgb * input.color.rgb;
    }
    
    output.color.a = gMaterial.color.a * textureColor.a * input.color.a;
    
    //textureのa値が閾値以下のときにPixelを破棄
    if (textureColor.a <= gMaterial.alphaReference)
    {
        discard;
    }
    
     //textureのa値が0のときにPixelを破棄
    if (textureColor.a == 0.0)
    {
        discard;
    }
    
     //output.colorのa値が0.0以下のときにPixelを破棄
    if (output.color.a <= 0.0)
    {
        discard;
    }
    return output;
}




