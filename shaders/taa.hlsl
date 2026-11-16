// taa.hlsl : TAA の解決 (リゾルブ) パス。
//   入力: シーンカラー (今フレーム, ジッタ付き) / 速度 / 履歴 (前フレームの解決結果)
//   出力: SV_Target0 = バックバッファ (RGBA8), SV_Target1 = 次フレームの履歴 (RGBA16F)
//   手順: 今のピクセルの 3x3 近傍から色の分布 (平均 ± 標準偏差, YCoCg) を求め → 速度で再投影した履歴をその範囲にクランプ → 今の色と混ぜる。
//   クランプのおかげで、再投影が外れた箇所 (遮蔽の解除・マテリアル/ライトの変更など) の古い色が残らない。

cbuffer ResolveCB : register(b0)
{
    float4 gRect;    // x0, y0, x1, y1 (px): シーンを描いた矩形。この外は読まない
    float4 gParams;  // x = TAA 有効, y = 履歴リセット (1 なら今の色だけ), z = 静止時の今フレーム重み, w = 動いている時の今フレーム重み
    float4 gScreen;  // 幅, 高さ, 1/幅, 1/高さ (px)
};

Texture2D<float4> gScene   : register(t0);
Texture2D<float2> gVelocity: register(t1);
Texture2D<float4> gHistory : register(t2);
SamplerState      gLinear  : register(s0); // linear / clamp

float4 ResolveVS(uint vid : SV_VertexID) : SV_POSITION
{
    float2 t = float2((vid << 1) & 2, vid & 2);
    return float4(t * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float3 RGBToYCoCg(float3 c)
{
    return float3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
                  0.5 * c.r - 0.5 * c.b,
                  -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);
}

float3 YCoCgToRGB(float3 c)
{
    return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}

struct PSOut
{
    float4 color   : SV_Target0;
    float4 history : SV_Target1;
};

PSOut ResolvePS(float4 svpos : SV_POSITION)
{
    const int2 p = int2(svpos.xy);
    const int2 lo = int2(gRect.xy);
    const int2 hi = int2(gRect.zw) - 1;

    const float3 cur = gScene.Load(int3(p, 0)).rgb;
    PSOut o;
    if (gParams.x < 0.5)
    {
        o.color = float4(cur, 1.0);
        o.history = float4(cur, 1.0);
        return o;
    }

    // 3x3 近傍の色分布 (YCoCg)
    float3 m1 = 0.0, m2 = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            const int2 q = clamp(p + int2(x, y), lo, hi);
            const float3 c = RGBToYCoCg(gScene.Load(int3(q, 0)).rgb);
            m1 += c;
            m2 += c * c;
        }
    }
    const float3 mean = m1 / 9.0;
    const float3 sigma = sqrt(max(m2 / 9.0 - mean * mean, 0.0));
    const float kGamma = 1.25; // 大きいほど履歴を信用する (ゴーストが増える)

    // 履歴を再投影して読む (ピクセル中心 + 速度)
    const float2 vel = gVelocity.Load(int3(p, 0)).xy;
    const float2 histPx = svpos.xy + vel;
    const bool inside = all(histPx >= gRect.xy + 0.5) && all(histPx <= gRect.zw - 0.5); // 矩形の縁 (バイリニアが外を読む) は使わない

    float alpha = 1.0; // 今フレームの重み
    float3 result = cur;
    if (gParams.y < 0.5 && inside)
    {
        const float3 hist = RGBToYCoCg(gHistory.SampleLevel(gLinear, histPx * gScreen.zw, 0).rgb);
        const float3 clamped = clamp(hist, mean - kGamma * sigma, mean + kGamma * sigma);
        alpha = lerp(gParams.z, gParams.w, saturate(length(vel))); // 1px 以上動いていれば今フレーム寄り (再投影のぼけを抑える)
        result = YCoCgToRGB(lerp(clamped, RGBToYCoCg(cur), alpha));
    }
    result = saturate(result);
    o.color = float4(result, 1.0);
    o.history = float4(result, 1.0);
    return o;
}
