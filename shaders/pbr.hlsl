// pbr.hlsl : ベイク済み IBL データ (Prefiltered Cube / BRDF LUT / Irradiance Cube) を使った PBR 球体 + スカイ描画
// 座標系は左手系 / Y-up。IBLBaker.cpp の規約と一致させること。

cbuffer SceneCB : register(b0)
{
    float4x4 gViewProj;     // ジッタ込み (ラスタライズ用)
    float4x4 gWorld;
    float4x4 gInvViewProj;  // ジッタ込みの逆行列 (スカイの視線方向)
    float4x4 gCurViewProj;  // ジッタ無し (速度の計算用)
    float4x4 gPrevViewProj; // 前フレームのジッタ無しビュー射影
    float4   gCameraPos;   // xyz
    float4   gAlbedo;      // rgb, a = reflectance (誘電体 F0 = 0.16 * reflectance^2)
    float4   gMaterial;    // x = metallic, y = roughness, z = diffuse intensity, w = specular intensity
    float4   gEnv;         // x = exposure (linear), y = env rotation (rad), z = max mip (= mipCount-1), w = sky lod
    float4   gFlags;       // x = use diffuse, y = use specular, z = multi-scatter, w = tonemap (0 ACES, 1 Reinhard, 2 none)
    float4   gRect;        // xy = シーン矩形の大きさ (px)
};

TextureCube<float4> gPrefiltered : register(t0);
Texture2D<float2>   gBrdfLut     : register(t1);
TextureCube<float4> gIrradiance  : register(t2); // 拡散光: コサイン畳み込み済み (= irradiance / π)
SamplerState        gSampler     : register(s0); // linear / clamp

static const float PI = 3.14159265359;

// ---------------------------------------------------------------- ユーティリティ
float3 RotateY(float3 v, float a)
{
    float c = cos(a), s = sin(a);
    return float3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

float3 ACESFilm(float3 x)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 LinearToSRGB(float3 c)
{
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(max(c, 1e-6), 1.0 / 2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, c));
}

float3 Present(float3 hdr)
{
    float3 c = hdr * gEnv.x;
    int mode = (int)(gFlags.w + 0.5);
    if (mode == 0)      c = ACESFilm(c);
    else if (mode == 1) c = c / (1.0 + c);
    else                c = saturate(c);
    return LinearToSRGB(c);
}

// ---------------------------------------------------------------- 速度 (TAA)
// 2 枚の MRT へ出力する: SV_Target0 = カラー (トーンマップ + sRGB 済み), SV_Target1 = 速度。
// 速度 = 「この点が前フレームでいた位置 - 今の位置」(ピクセル単位, 画面は y 下向き)。TaaResolver が履歴を引く位置に足す。
struct PSOut
{
    float4 color : SV_Target0;
    float2 vel   : SV_Target1;
};

float2 MotionPixels(float4 curClip, float4 prevClip)
{
    float2 motion = float2(0.0, 0.0); // 画面外 / 背後は 0 (履歴は TaaResolver の範囲チェックで捨てられる)
    if (curClip.w > 1e-5 && prevClip.w > 1e-5)
    {
        float2 cur  = curClip.xy / curClip.w;
        float2 prev = prevClip.xy / prevClip.w;
        motion = (prev - cur) * float2(0.5 * gRect.x, -0.5 * gRect.y);
    }
    return motion;
}

// ---------------------------------------------------------------- 球体
struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
};
struct VSOut
{
    float4 pos   : SV_POSITION;
    float3 posWS : TEXCOORD0;
    float3 nrmWS : TEXCOORD1;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 w = mul(float4(i.pos, 1.0), gWorld);
    o.posWS = w.xyz;
    o.pos   = mul(w, gViewProj);
    o.nrmWS = mul(i.nrm, (float3x3)gWorld);
    return o;
}

PSOut PSMain(VSOut i)
{
    float3 albedo    = gAlbedo.rgb;
    float  metallic  = gMaterial.x;
    float  roughness = clamp(gMaterial.y, 0.0, 1.0);

    float3 N = normalize(i.nrmWS);
    float3 V = normalize(gCameraPos.xyz - i.posWS);
    float  NdotV = saturate(dot(N, V));
    float3 R = reflect(-V, N);

    float  dielectricF0 = 0.16 * gAlbedo.a * gAlbedo.a;
    float3 F0 = lerp(dielectricF0.xxx, albedo, metallic);
    float3 diffuseColor = albedo * (1.0 - metallic);

    // --- Split-sum スペキュラ
    float2 ab = gBrdfLut.SampleLevel(gSampler, float2(NdotV, roughness), 0);
    float3 specColor = F0 * ab.x + ab.y;
    float  lod = roughness * gEnv.z;
    float3 prefiltered = gPrefiltered.SampleLevel(gSampler, RotateY(R, gEnv.y), lod).rgb;
    float3 specular = prefiltered * specColor;
    if (gFlags.z > 0.5) // 多重散乱エネルギー補償 (Fdez-Aguera)
    {
        float3 Fms = 1.0 + F0 * (1.0 / max(ab.x + ab.y, 1e-4) - 1.0);
        specular *= Fms;
    }
    specular *= gMaterial.w * gFlags.y;

    // --- Irradiance Cube 拡散光 (irradiance / π) * albedo
    float3 irradianceOverPi = gIrradiance.SampleLevel(gSampler, RotateY(N, gEnv.y), 0).rgb;
    float3 diffuse = diffuseColor * (1.0 - specColor) * irradianceOverPi * gMaterial.z * gFlags.x;

    PSOut o;
    o.color = float4(Present(diffuse + specular), 1.0);
    // オブジェクトは静止 (ワールド行列は単位)。カメラの動きだけが速度になる
    o.vel = MotionPixels(mul(float4(i.posWS, 1.0), gCurViewProj), mul(float4(i.posWS, 1.0), gPrevViewProj));
    return o;
}

// ---------------------------------------------------------------- スカイ (フルスクリーン三角形)
struct SkyOut
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

SkyOut SkyVS(uint vid : SV_VertexID)
{
    SkyOut o;
    float2 t = float2((vid << 1) & 2, vid & 2);          // (0,0) (2,0) (0,2)
    o.pos = float4(t * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
    o.ndc = o.pos.xy;
    return o;
}

PSOut SkyPS(SkyOut i)
{
    float4 wp  = mul(float4(i.ndc, 1.0, 1.0), gInvViewProj);
    float3 dir = normalize(wp.xyz / wp.w - gCameraPos.xyz);
    float3 c   = gPrefiltered.SampleLevel(gSampler, RotateY(dir, gEnv.y), gEnv.w).rgb;
    PSOut o;
    o.color = float4(Present(c), 1.0);
    // 無限遠: 方向ベクトル (w = 0) だけで投影する = カメラの回転だけが速度になる (平行移動は効かない)
    o.vel = MotionPixels(mul(float4(dir, 0.0), gCurViewProj), mul(float4(dir, 0.0), gPrevViewProj));
    return o;
}
