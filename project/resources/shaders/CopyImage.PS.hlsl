#include "CopyImage.hlsli"


Texture2D<float32_t4> gTexture : register(t0);
// 深度バッファを受けとる
Texture2D<float32_t4> gDepthTexture : register(t1);

// ノイズテクスチャを受け取る
Texture2D<float32_t4> gNoiseTexture : register(t2);

SamplerState gSampler : register(s0);

// 疑似乱数生成関数
float rand(float2 co)
{
    return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

// C++から送られてくる「定数」を受け取る箱
cbuffer PostEffectParam : register(b0)
{
    int effectType; // エフェクトの種類
    float vignetteRadius; // ヴィネットの半径
    float vignetteSoftness; // ヴィネットのぼかし具合
    float blurIntensity; // ぼかしの強さ
    float dissolveThreshold; // ディゾルブのしきい値
    float dissolveEdgeWidth; // ディゾルブのエッジの幅 (※ここの全角スペースを修正！)
    float pad1; // サイズ合わせ用
    float pad2; // サイズ合わせ用
    float3 edgeColor;
    float pad3;
    float3 noneColor;
    float time;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 texColor = gTexture.Sample(gSampler, input.texcoord);
    
    // effectType の値によって色を切り替える
    if (effectType == 1)
    {
        // 1: モノクロ
        float gray = texColor.r * 0.2125f + texColor.g * 0.7154f + texColor.b * 0.0721f;
        output.color = float4(gray, gray, gray, texColor.a);
    }
    else if (effectType == 2)
    {
        // 2: 色反転
        output.color = float4(1.0f - texColor.r, 1.0f - texColor.g, 1.0f - texColor.b, texColor.a);
    }
    else if (effectType == 3)
    {
        // 3: セピア (NTSC規格などでよく使われるセピア化の係数)
        float value = texColor.r * 0.2125f + texColor.g * 0.7154f + texColor.b * 0.0721f;
        output.color.rgb = value * float32_t3(1.0f, 74.0f / 107.0f, 43.0f / 107.0f);
        output.color.a = texColor.a;
    }
    else if (effectType == 4)
    {
        // 4: ヴィネッティング
        // UV座標の中心は (0.5, 0.5)
        float2 center = float2(0.5f, 0.5f);
        
        // 中心からの距離を計算（中心なら0.0、四隅なら最大約0.7）
        float dist = distance(input.texcoord, center);
        
        // smoothstep(min, max, x) で滑らかなグラデーションを作る
        // 例: 距離0.3までは明るいまま、0.8に向けて徐々に暗くする
        float vignette = 1.0f - smoothstep(vignetteRadius, vignetteRadius + vignetteSoftness, dist);
        
        // 元の色にヴィネット係数（0.0～1.0）を掛け算して暗くする
        output.color = float4(texColor.rgb * vignette, texColor.a);
    }
    else if (effectType == 5)
    {
        // 5: ボックスフィルタ (3x3ピクセルの平均ブラー)
        
        // 1. テクスチャの解像度を取得し、1ピクセルあたりのUVサイズを計算する
        uint width, height;
        gTexture.GetDimensions(width, height);
        float2 uvStep = float2(1.0f / width, 1.0f / height);

        // ぼかしの強さ（2.0fや3.0fにすると、より遠くのピクセルを拾って強くぼけます）
        float intensity = 1.5f;

        // 色を足し合わせるための箱を用意
        float4 colorSum = 0;
        
        // 2. 中心を含む周囲9ピクセル（タテ3 × ヨコ3）の色をサンプリングして足す
        for (int y = -1; y <= 1; ++y)
        {
            for (int x = -1; x <= 1; ++x)
            {
                // 現在のUV座標から少しだけズラした位置を計算
                float2 offset = float2(x, y) * uvStep * blurIntensity;
                colorSum += gTexture.Sample(gSampler, input.texcoord + offset);
            }
        }
        
        // 3. 9回足したので、最後に9で割って「平均色」にする
        output.color = colorSum / 9.0f;
    }
    else if (effectType == 6)
    {
        // 6: 5x5 ボックスフィルタ
        uint width, height;
        gTexture.GetDimensions(width, height);
        float2 uvStep = float2(1.0f / width, 1.0f / height);
        
        float4 colorSum = float4(0.0f, 0.0f, 0.0f, 0.0f);
        
        // ★変更点1：ループの範囲を -2 から 2 に広げる
        for (int y = -2; y <= 2; ++y)
        {
            for (int x = -2; x <= 2; ++x)
            {
                float2 offset = float2(x, y) * uvStep * blurIntensity;
                colorSum += gTexture.Sample(gSampler, input.texcoord + offset);
            }
        }
        
        // ★変更点2：5x5 = 25回足したので、25.0fで割る
        output.color = colorSum / 25.0f;
    }
    else if (effectType == 7)
    {
        // 7: ガウシアンブラー (5x5)
        uint width, height;
        gTexture.GetDimensions(width, height);
        float2 uvStep = float2(1.0f / width, 1.0f / height);

        // ガウシアンカーネル（5x5 = 25個の重み）
        // 中心が一番大きく(0.1406)、端に行くほど小さくなる(0.0039)
        // 合計するとピッタリ 1.0 になります
        float kernel[25] =
        {
            0.0039f, 0.0156f, 0.0234f, 0.0156f, 0.0039f,
            0.0156f, 0.0625f, 0.0937f, 0.0625f, 0.0156f,
            0.0234f, 0.0937f, 0.1406f, 0.0937f, 0.0234f,
            0.0156f, 0.0625f, 0.0937f, 0.0625f, 0.0156f,
            0.0039f, 0.0156f, 0.0234f, 0.0156f, 0.0039f
        };

        float4 colorSum = float4(0.0f, 0.0f, 0.0f, 0.0f);
        
        // ループで周囲のピクセルを拾い、対応する「重み」を掛けて足し合わせる
        for (int y = -2; y <= 2; ++y)
        {
            for (int x = -2; x <= 2; ++x)
            {
                float2 offset = float2(x, y) * uvStep * blurIntensity;
                
                // 配列のインデックス（0～24）を計算
                int index = (y + 2) * 5 + (x + 2);
                
                // ピクセルの色に「重み（kernel）」を掛け算する
                colorSum += gTexture.Sample(gSampler, input.texcoord + offset) * kernel[index];
            }
        }
        
        // 重みの合計はすでに1.0になっているので、割り算は不要！
        output.color = colorSum;
    }
    else if (effectType==8)
    {
        // 8: エッジ検出 (Sobel Filter)
        uint width, height;
        gTexture.GetDimensions(width, height);
        float2 uvStep = float2(1.0f / width, 1.0f / height);

        // 横方向（X）の輝度の差を測るカーネル
        float sobelX[3][3] =
        {
            { -1.0f, 0.0f, 1.0f },
            { -2.0f, 0.0f, 2.0f },
            { -1.0f, 0.0f, 1.0f }
        };

        // 縦方向（Y）の輝度の差を測るカーネル
        float sobelY[3][3] =
        {
            { -1.0f, -2.0f, -1.0f },
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 2.0f, 1.0f }
        };

        float xValue = 0.0f;
        float yValue = 0.0f;

        // 周囲9ピクセルをサンプリング
        for (int y = -1; y <= 1; ++y)
        {
            for (int x = -1; x <= 1; ++x)
            {
                float2 offset = float2(x, y) * uvStep;
                
                // 輝度ではなく、深度テクスチャの「.r」を直接取得する！
                float depth = gDepthTexture.Sample(gSampler, input.texcoord + offset).r;

                // XとYそれぞれのカーネルの重みを掛けて足し合わせる
                // ※配列のインデックスがマイナスにならないよう [y + 1][x + 1] とする
                xValue += depth * sobelX[y + 1][x + 1];
                yValue += depth * sobelY[y + 1][x + 1];
            }
        }

        // ピタゴラスの定理（三平方の定理）で、XとYのベクトルの長さを計算
        // これが「エッジの強さ（0.0 ～ 1.0以上）」になります
        float edge = sqrt(xValue * xValue + yValue * yValue);

        // 数値を 0.0 ～ 1.0 の間に強制的に収める（はみ出させない）関数
        float weight = saturate(edge * 500.0f);

        // スライドの処理：weightが大きい（＝エッジが強い）ほど暗く表示する合成方法
        // (1.0f - weight) なので、エッジがない場所(0.0)は元の色(1.0倍)になり、
        // エッジが強い場所(1.0)は真っ黒(0.0倍)になります。
        output.color.rgb = (1.0f - weight) * texColor.rgb;
        output.color.a = 1.0f;
    }
    else if (effectType == 9)
    {
        // ブラーの収束点（とりあえず画面中央の 0.5, 0.5 に固定）
        float2 center = float2(0.5f, 0.5f);
        
        // 現在のピクセル(UV)から、中心点に向かう方向ベクトルを計算
        float2 direction = center - input.texcoord;

        // ぼかしの強さ（C++から送られてくる blurIntensity を使う）
        // そのままだと強すぎるので、0.01倍して調整しやすい値にする
        float strength = blurIntensity * 0.01f;

        // サンプリング回数（多いほど滑らかになるが重くなる。10〜16回程度が標準）
        const int NUM_SAMPLES = 10;
        
        float4 colorSum = float4(0.0f, 0.0f, 0.0f, 0.0f);

        // 中心に向かって直線上を少しずつ進みながら色を拾う
        for (int i = 0; i < NUM_SAMPLES; ++i)
        {
            // 0.0 〜 1.0 の割合を計算
            float percent = (float) i / (float) NUM_SAMPLES;
            
            // ずらす距離を計算
            float2 offset = direction * strength * percent;
            
            // 色を取得して足し合わせる
            colorSum += gTexture.Sample(gSampler, input.texcoord + offset);
        }

        // 最後にサンプリングした回数で割って平均化する
        output.color = colorSum / (float) NUM_SAMPLES;
    }
    else if (effectType == 10)
    {
        // ノイズテクスチャから値を取得（モノクロ画像なので .r の明るさを使う）
        float noise = gNoiseTexture.Sample(gSampler, input.texcoord).r;
        
        // ① ノイズの値が、しきい値より小さければ「消える（黒にする）」
        if (noise < dissolveThreshold)
        {
            output.color = float4(noneColor, 1.0f);
            
        }
        else
        {
            // まずは元のテクスチャの色をそのまま出力にセットする
            output.color = texColor;

            // Edgeっぽさを算出 (smoothstepで滑らかな境界を作る)
            // mask の値が threshold に近いほど 1.0(エッジ強い)、離れるほど 0.0 になる
            float edge = 1.0f - smoothstep(dissolveThreshold, dissolveThreshold + dissolveEdgeWidth, noise);

            // Edgeっぽいほど指定した色(edgeColor)を加算（+=）して光らせる！
            output.color.rgb += edge * edgeColor;
        }
    }
    else if (effectType == 11)
    {
        
        // 単純に UV 座標だけをシード（種）にすると、毎回同じ乱数になってしまい「止まった画像」になります。
        // そこで、UV座標に「時間(time)」を足すことで、毎フレーム違う乱数が生まれるようにします！
        
        float2 seed = input.texcoord + float2(time, time);
        
        // 乱数を生成 (0.0 ～ 1.0)
        float noiseValue = rand(seed);
        
        // 白黒のノイズとして出力
        output.color = float4(noiseValue, noiseValue, noiseValue, 1.0f);
        
        // 💡 おまけ：もし「元の画像の上にうっすらノイズを乗せたい」場合はこう書きます
        // output.color = texColor + float4(noiseValue, noiseValue, noiseValue, 0.0f) * 0.2f;
    }
    else if (effectType == 12)
    {
        uint width, height;
        gTexture.GetDimensions(width, height);
        float2 uvStep = float2(1.0f / width, 1.0f / height);

        float smoothAmount = max(1.0f, blurIntensity) * (1.0f + 0.12f * sin(time * 3.0f));
        float2 offset = uvStep * smoothAmount;

        float4 smoothColor = texColor * 0.36f;
        smoothColor += gTexture.Sample(gSampler, input.texcoord + float2( offset.x, 0.0f)) * 0.10f;
        smoothColor += gTexture.Sample(gSampler, input.texcoord + float2(-offset.x, 0.0f)) * 0.10f;
        smoothColor += gTexture.Sample(gSampler, input.texcoord + float2(0.0f,  offset.y)) * 0.10f;
        smoothColor += gTexture.Sample(gSampler, input.texcoord + float2(0.0f, -offset.y)) * 0.10f;
        smoothColor += gTexture.Sample(gSampler, input.texcoord + float2( offset.x,  offset.y)) * 0.06f;
        smoothColor += gTexture.Sample(gSampler, input.texcoord + float2(-offset.x,  offset.y)) * 0.06f;
        smoothColor += gTexture.Sample(gSampler, input.texcoord + float2( offset.x, -offset.y)) * 0.06f;
        smoothColor += gTexture.Sample(gSampler, input.texcoord + float2(-offset.x, -offset.y)) * 0.06f;

        float dist = distance(input.texcoord, float2(0.5f, 0.5f));
        float pulse = 0.03f * sin(time * 2.0f);
        float vignette = 1.0f - smoothstep(vignetteRadius + pulse, vignetteRadius + vignetteSoftness + pulse, dist);
        vignette = lerp(0.22f, 1.0f, vignette);

        output.color = float4(smoothColor.rgb * vignette, texColor.a);
    }
    else if (effectType == 13)
    {
        // 13: フォールド波干渉 / ヴァールシンドローム (Chromatic Aberration + Glitch + Scanline)
        
        // 1. グリッチ (Glitch Offset)
        // 特定の時間帯だけUVを横にずらす
        float glitchOffset = 0.0f;
        float glitchTime = time * 5.0f;
        if (frac(glitchTime) > 0.85f)
        {
            // ノイズテクスチャからランダムなズレを取得
            float noiseVal = gNoiseTexture.Sample(gSampler, float2(input.texcoord.y * 10.0f, time)).r;
            glitchOffset = (noiseVal - 0.5f) * 0.1f * sin(time * 20.0f);
        }
        
        float2 uv = input.texcoord + float2(glitchOffset, 0.0f);

        // 2. 色収差 (Chromatic Aberration)
        // 時間経過でRGBのズレ幅が変動する
        float abAmount = 0.015f + 0.01f * sin(time * 8.0f);
        float2 center = float2(0.5f, 0.5f);
        float2 dir = uv - center;
        
        float r = gTexture.Sample(gSampler, uv + dir * abAmount).r;
        float g = gTexture.Sample(gSampler, uv).g;
        float b = gTexture.Sample(gSampler, uv - dir * abAmount).b;
        
        float4 finalColor = float4(r, g, b, 1.0f);
        
        // 3. 走査線 (Scanlines)
        // 画面に横線を入れる
        float scanline = sin(uv.y * 800.0f + time * 10.0f) * 0.04f;
        finalColor.rgb -= scanline;
        
        // 全体的に少しサイバーな青みを足す (フォールド波っぽさ)
        finalColor.rgb += float3(0.0f, 0.05f, 0.1f) * (1.0f + sin(time * 5.0f)) * 0.5f;
        
        output.color = finalColor;
    }
    else if (effectType == 14)
    {
        // 14: 通信切断・システムシャットダウン (SF CRT Shutdown & Glitch)
        // dissolveThreshold: シャットダウン進行度 (0.0: 切断開始 ～ 1.0: 完全暗転)
        // blurIntensity: グリッチ・走査線強度 (0.0 ～ 1.0)
        float progress = saturate(dissolveThreshold);
        float glitchStrength = saturate(blurIntensity);

        // 1. 完全暗転フェーズ (0.96以上)
        if (progress >= 0.96f)
        {
            output.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
            return output;
        }

        float2 uv = input.texcoord;

        // 2. ブロックノイズと水平スライスグリッチ
        // ランダムな水平帯ごとにUVをずらす
        float sliceY = floor(uv.y * 36.0f);
        float sliceNoise = rand(float2(sliceY, floor(time * 24.0f)));
        float blockTrigger = step(0.60f - glitchStrength * 0.35f, sliceNoise);
        float hShift = (rand(float2(sliceNoise, time * 15.0f)) - 0.5f) * 0.12f * blockTrigger * glitchStrength;
        uv.x += hShift;

        // 微小な走査ジッター
        uv.x += (rand(float2(time * 60.0f, uv.y)) - 0.5f) * 0.015f * glitchStrength;

        // 3. ブラウン管テレビ電源OFF（CRT Collapse）
        // progress: 0.00 ～ 0.20 : 通信切断グリッチ先行（画面全体でノイズ・走査線が走る）
        // progress: 0.20 ～ 0.75 : 上下から中央に向かって急速に潰れる（Vertical Collapse）
        // progress: 0.75 ～ 0.94 : 水平の細い発光線が左右から中央へ「プツッ」と収束（Horizontal Pinch / Snap）
        // progress: 0.94 ～ 1.00 : 中心の光点が消えて真っ暗（Fade to Black）

        float vProgress = saturate((progress - 0.18f) / 0.55f);
        // 上下開口度 (1.0 -> 0.003)
        // キレのあるイージング (昔のテレビ特有のシュッと中央へ吸い込まれる挙動)
        float vOpen = pow(1.0f - vProgress, 2.5f);
        vOpen = max(vOpen, 0.003f);

        float hProgress = saturate((progress - 0.72f) / 0.22f);
        // 左右開口度 (1.0 -> 0.0)
        float hOpen = pow(1.0f - hProgress, 3.0f);

        float distY = abs(uv.y - 0.5f);
        float distX = abs(uv.x - 0.5f);

        // 範囲外は真っ暗（上下・左右のブラックシャッター）
        if (distY > vOpen * 0.5f || (vProgress > 0.96f && distX > hOpen * 0.5f))
        {
            output.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
            return output;
        }

        // 潰れた画面内部のUV（上下方向へ圧縮マッピング、中央付近の画像が凝縮）
        float2 crtUv = uv;
        if (vProgress > 0.0f)
        {
            crtUv.y = 0.5f + (uv.y - 0.5f) / max(vOpen, 0.005f) * 0.5f;
            crtUv.y = saturate(crtUv.y);
        }

        // 4. 色収差 (RGB Split)
        float splitAmount = (0.012f + 0.025f * vProgress) * glitchStrength;
        float r = gTexture.Sample(gSampler, crtUv + float2(splitAmount, 0.0f)).r;
        float g = gTexture.Sample(gSampler, crtUv).g;
        float b = gTexture.Sample(gSampler, crtUv - float2(splitAmount, 0.0f)).b;
        float4 baseCol = float4(r, g, b, 1.0f);

        // 5. 走査線（スキャンライン）
        float scanline = sin(uv.y * 720.0f + time * 30.0f) * (0.15f + 0.25f * glitchStrength);
        baseCol.rgb -= scanline;

        // 6. 白黒・RGB粒子ノイズ
        float grain = (rand(uv + float2(time * 33.0f, time * 77.0f)) - 0.5f) * 0.22f * glitchStrength;
        baseCol.rgb += grain;

        // 7. ブラウン管蛍光体集中（中央線に近づくほど高輝度・ホワイト/シアン化）
        float phosphorIntensity = 1.0f + (1.0f - vOpen) * 2.8f;
        float edgeGlow = smoothstep(0.0f, 1.0f, 1.0f - (distY / (vOpen * 0.5f + 1e-5f)));
        float lineCore = pow(edgeGlow, 3.5f) * vProgress * 2.0f;

        baseCol.rgb *= phosphorIntensity;
        baseCol.rgb += float3(0.4f, 0.85f, 1.0f) * lineCore;

        // 水平収束フェーズ（左右から点へ消える瞬間）
        if (hProgress > 0.0f)
        {
            float pointGlow = saturate(1.0f - distX / (hOpen * 0.5f + 1e-5f));
            baseCol.rgb += float3(1.0f, 1.0f, 1.0f) * pow(pointGlow, 2.0f) * 2.5f;
            baseCol.rgb *= (1.0f - saturate((progress - 0.92f) / 0.04f));
        }

        output.color = float4(baseCol.rgb, 1.0f);
    }
    else
    {
        // 0: ノーマル（そのまま）
        output.color = texColor;
    }
    return output;
}
