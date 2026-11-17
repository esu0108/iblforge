// ibl_bake.hlsl : IBL ベイクの GPU (Compute Shader) 実装。IBLBaker.cpp (CPU 版) と同じアルゴリズム・座標系。
//   - EquirectToCubeCS : Equirect -> Cube (2x2 スーパーサンプリング)
//   - DownsampleCS     : Cube の mip 生成 (2x2 ボックス)
//   - PrefilterCopyCS  : Prefiltered Cube の mip0 (roughness = 0)
//   - PrefilterCS      : GGX 重点サンプリングによる Prefiltered Specular
//   - IrradianceCS     : コサイン畳み込み (= irradiance / π)
//   - BrdfLutCS        : Split-sum BRDF LUT
//
// 座標系: 左手系 / Y-up (D3D)。Cube 面順は +X,-X,+Y,-Y,+Z,-Z。
// サンプル列 (gSamples) は CPU 側で IBLBaker.cpp の BuildXxxSampleTable が生成したものを使う (CPU 版と同一)。

cbuffer Params : register(b0)   // root constants (12 DWORD)。IBLBakerGpu.cpp の GpuParams と一致させること
{
    uint  gSize;          // 出力の一辺
    uint  gRowOffset;     // この dispatch が担当する行の開始位置 (TDR 回避のため分割 dispatch する)
    uint  gSampleCount;
    uint  gSampleOffset;  // gSamples 内の開始位置
    uint  gEqWidth;
    uint  gEqHeight;
    uint  gPad0;
    uint  gPad1;
    float gRoughness;
    float gMaxSample;     // > 0 ならサンプルの最大成分をクランプ
    float gPad2;
    float gPad3;
};

Texture2D<float4>        gEquirect : register(t0);
TextureCube<float4>      gSrcCube  : register(t1);
Texture2DArray<float4>   gMipSrc   : register(t2);   // DownsampleCS 用 (1 mip, 6 slice)
StructuredBuffer<float4> gSamples  : register(t3);   // xyz = タンジェント空間の L, w = 入力キューブの mip
RWTexture2DArray<float4> gOut      : register(u0);
SamplerState             gSampler  : register(s0);   // linear / clamp (D3D のキューブサンプリングは面をまたいでシームレス)

static const float PI = 3.14159265359;

// ---------------------------------------------------------------- 共通
// u,v は [-1,1]、v は画像の下方向が +。
float3 FaceUVToDir(uint face, float u, float v)
{
    // 戻り値は 1 か所で返す (早期 return の連続だと、コンパイラが「未初期化の可能性」と警告する)
    float3 d = float3(-u, -v, -1.0); // face == 5
    if      (face == 0) d = float3( 1.0, -v, -u);
    else if (face == 1) d = float3(-1.0, -v,  u);
    else if (face == 2) d = float3(   u, 1.0,  v);
    else if (face == 3) d = float3(   u, -1.0, -v);
    else if (face == 4) d = float3(   u, -v, 1.0);
    return d;
}

float RadicalInverse(uint i)
{
    return float(reversebits(i)) * 2.3283064365386963e-10;
}

float3 ClampSample(float3 c)
{
    if (gMaxSample > 0.0)
    {
        float mx = max(c.x, max(c.y, c.z));
        if (mx > gMaxSample) c *= gMaxSample / mx;
    }
    return c;
}

void MakeBasis(float3 N, out float3 T, out float3 B)
{
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

float3 EquirectTexel(int x, int y)
{
    int w = (int)gEqWidth, h = (int)gEqHeight;
    // 経度はラップ。呼び出し側の x は [-1, w] の範囲なので、整数の剰余 (遅い) を使わず 1 回の加減算で足りる
    x = x < 0 ? x + w : (x >= w ? x - w : x);
    y = clamp(y, 0, h - 1);         // 緯度はクランプ
    return gEquirect.Load(int3(x, y, 0)).rgb;
}

// CPU 版 SampleEquirect と同じ (自前の bilinear)。Equirect は R32G32B32A32_FLOAT のため HW フィルタに頼らない。
float3 SampleEquirect(float3 d)
{
    float u = 0.5 + atan2(d.x, d.z) * (1.0 / (2.0 * PI));
    float v = acos(clamp(d.y, -1.0, 1.0)) * (1.0 / PI);
    float fx = u * gEqWidth - 0.5, fy = v * gEqHeight - 0.5;
    int x0 = (int)floor(fx), y0 = (int)floor(fy);
    float tx = fx - x0, ty = fy - y0;
    float3 a = lerp(EquirectTexel(x0, y0),     EquirectTexel(x0 + 1, y0),     tx);
    float3 b = lerp(EquirectTexel(x0, y0 + 1), EquirectTexel(x0 + 1, y0 + 1), tx);
    return lerp(a, b, ty);
}

float2 TexelUV(uint x, uint y)
{
    return float2(2.0 * (x + 0.5) / gSize - 1.0, 2.0 * (y + 0.5) / gSize - 1.0);
}

// ---------------------------------------------------------------- Equirect -> Cube
[numthreads(8, 8, 1)]
void EquirectToCubeCS(uint3 id : SV_DispatchThreadID)
{
    uint x = id.x, y = id.y + gRowOffset, face = id.z;
    if (x >= gSize || y >= gSize) return;

    float3 acc = 0;
    [unroll] for (int sy = 0; sy < 2; ++sy)
    [unroll] for (int sx = 0; sx < 2; ++sx)
    {
        float u = 2.0 * (x + 0.25 + 0.5 * sx) / gSize - 1.0;
        float v = 2.0 * (y + 0.25 + 0.5 * sy) / gSize - 1.0;
        acc += SampleEquirect(normalize(FaceUVToDir(face, u, v)));
    }
    gOut[uint3(x, y, face)] = float4(acc * 0.25, 1.0);
}

// ---------------------------------------------------------------- mip 生成
[numthreads(8, 8, 1)]
void DownsampleCS(uint3 id : SV_DispatchThreadID)   // gSize = 出力 mip の一辺
{
    if (id.x >= gSize || id.y >= gSize) return;
    int2 p = int2(id.xy) * 2;
    int  f = (int)id.z;
    float4 c = gMipSrc.Load(int4(p,               f, 0))
             + gMipSrc.Load(int4(p + int2(1, 0),  f, 0))
             + gMipSrc.Load(int4(p + int2(0, 1),  f, 0))
             + gMipSrc.Load(int4(p + int2(1, 1),  f, 0));
    gOut[id] = float4(c.rgb * 0.25, 1.0);
}

// ---------------------------------------------------------------- Prefiltered Specular
[numthreads(8, 8, 1)]
void PrefilterCopyCS(uint3 id : SV_DispatchThreadID)    // roughness = 0 : 入力 mip0 を (必要なら再サンプルして) そのまま出力
{
    uint x = id.x, y = id.y + gRowOffset, face = id.z;
    if (x >= gSize || y >= gSize) return;
    float2 uv = TexelUV(x, y);
    gOut[uint3(x, y, face)] = float4(gSrcCube.SampleLevel(gSampler, FaceUVToDir(face, uv.x, uv.y), 0).rgb, 1.0);
}

[numthreads(8, 8, 1)]
void PrefilterCS(uint3 id : SV_DispatchThreadID)
{
    uint x = id.x, y = id.y + gRowOffset, face = id.z;
    if (x >= gSize || y >= gSize) return;

    float2 uv = TexelUV(x, y);
    float3 N = normalize(FaceUVToDir(face, uv.x, uv.y));
    float3 T, B;
    MakeBasis(N, T, B);

    float3 sum = 0;
    float  weight = 0;
    [loop] for (uint i = 0; i < gSampleCount; ++i)
    {
        float4 s = gSamples[gSampleOffset + i];
        float3 L = T * s.x + B * s.y + N * s.z;
        float3 c = ClampSample(gSrcCube.SampleLevel(gSampler, L, s.w).rgb);
        sum    += c * s.z;      // s.z = NdotL
        weight += s.z;
    }
    float3 result = weight > 0.0 ? sum / weight : gSrcCube.SampleLevel(gSampler, N, 0).rgb;
    gOut[uint3(x, y, face)] = float4(result, 1.0);
}

// ---------------------------------------------------------------- Irradiance
[numthreads(8, 8, 1)]
void IrradianceCS(uint3 id : SV_DispatchThreadID)
{
    uint x = id.x, y = id.y + gRowOffset, face = id.z;
    if (x >= gSize || y >= gSize) return;

    float2 uv = TexelUV(x, y);
    float3 N = normalize(FaceUVToDir(face, uv.x, uv.y));
    float3 T, B;
    MakeBasis(N, T, B);

    float3 sum = 0;
    [loop] for (uint i = 0; i < gSampleCount; ++i)
    {
        float4 s = gSamples[gSampleOffset + i];
        sum += ClampSample(gSrcCube.SampleLevel(gSampler, T * s.x + B * s.y + N * s.z, s.w).rgb);
    }
    gOut[uint3(x, y, face)] = float4(sum / (float)gSampleCount, 1.0);
}

// ---------------------------------------------------------------- BRDF LUT (split-sum)
[numthreads(8, 8, 1)]
void BrdfLutCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x, j = id.y + gRowOffset;       // i = NdotV, j = roughness
    if (i >= gSize || j >= gSize) return;

    float roughness = (j + 0.5) / gSize;
    float alpha = roughness * roughness;
    float a2 = alpha * alpha;
    float k = alpha * 0.5;                       // IBL 用 Schlick-GGX
    float NdotV = max((i + 0.5) / gSize, 1e-3);
    float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    float A = 0.0, B = 0.0;
    [loop] for (uint n = 0; n < gSampleCount; ++n)
    {
        float xi1 = (n + 0.5) / gSampleCount;
        float xi2 = RadicalInverse(n);
        float phi = 2.0 * PI * xi1;
        float cosTheta = sqrt((1.0 - xi2) / (1.0 + (a2 - 1.0) * xi2));
        float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
        float3 H = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
        float VdotH = dot(V, H);
        float3 L = H * (2.0 * VdotH) - V;
        float NdotL = L.z, NdotH = H.z;
        if (NdotL > 0.0 && VdotH > 0.0)
        {
            float Gv = NdotV / (NdotV * (1.0 - k) + k);
            float Gl = NdotL / (NdotL * (1.0 - k) + k);
            float Gvis = (Gv * Gl) * VdotH / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * Gvis;
            B += Fc * Gvis;
        }
    }
    gOut[uint3(i, j, 0)] = float4(A / gSampleCount, B / gSampleCount, 0.0, 0.0);
}
