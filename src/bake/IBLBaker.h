// IBLBaker.h : HDRI からの IBL データ生成コア (プラットフォーム非依存の純C++)
//   - Equirect -> Cube 変換
//   - GGX 重点サンプリングによる Prefiltered Specular Cube Map
//   - Split-Sum BRDF LUT
//   - 拡散光用 Irradiance Cube Map (コサイン畳み込み済み, 1/π 込み)
//
// 座標系: 左手系 / Y-up (Direct3D 準拠)
//   Equirect: u = 0.5 + atan2(x, z) / 2π,  v = acos(y) / π   (画像中央 = +Z 方向)
//   Cube 面 : D3D の TextureCube と同じ (+X,-X,+Y,-Y,+Z,-Z)
#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

namespace ibl
{
	struct Vec3
	{
		float x = 0.0f, y = 0.0f, z = 0.0f;
	};
	inline Vec3 operator+( Vec3 a, Vec3 b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	inline Vec3 operator-( Vec3 a, Vec3 b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	inline Vec3 operator*( Vec3 a, float s ) { return { a.x * s, a.y * s, a.z * s }; }
	inline Vec3& operator+=( Vec3& a, Vec3 b ) { a = a + b; return a; }

	struct EquirectImage
	{
		uint32_t width = 0, height = 0;
		std::vector<Vec3> pixels; // リニア RGB, 行優先, 上が +Y
	};

	struct CubeMip
	{
		uint32_t size = 0;
		std::vector<Vec3> face[6]; // size*size
	};
	struct CubeMap
	{
		std::vector<CubeMip> mips; // mips[0] が最大
	};

	// 出力する Cubemap (Prefiltered / Irradiance) の DDS 形式。BRDF LUT は常に R16G16_FLOAT。
	enum class CubeFormat
	{
		Float16,   // R16G16B16A16_FLOAT (既定。HDR を十分な精度で保持)
		Float32,   // R32G32B32A32_FLOAT (無劣化だがサイズは 2 倍)
		R11G11B10, // R11G11B10_FLOAT    (サイズ 1/2。精度は低め・アルファ無し)
		BC6H       // BC6H_UF16          (サイズ 1/8 (対 F16)。非可逆圧縮。符号なし HDR 専用)
	};
	inline const char* CubeFormatName( CubeFormat f )
	{
		switch( f )
		{
		case CubeFormat::Float16:   return "RGBA16F";
		case CubeFormat::Float32:   return "RGBA32F";
		case CubeFormat::R11G11B10: return "R11G11B10F";
		case CubeFormat::BC6H:      return "BC6H";
		}
		return "?";
	}

	// ベイクのキャンセル要求を検出したときに進捗コールバックなどから投げる。RunBake が捕まえて「キャンセル」として扱う。
	struct BakeCancelled : std::runtime_error
	{
		BakeCancelled() : std::runtime_error( "Canceled" ) {}
	};

	struct BakeSettings
	{
		uint32_t sourceCubeSize = 512;  // 中間キューブ (Prefilter の入力) の一辺
		uint32_t prefilterSize = 256;  // 出力 Prefiltered Cube の mip0 の一辺 (2の累乗)
		uint32_t prefilterSamples = 1024; // GGX 重点サンプリング数
		uint32_t lutSize = 256;  // BRDF LUT の一辺
		uint32_t lutSamples = 1024;
		uint32_t irradianceSize = 32;   // Irradiance Cube の一辺 (ミップ無し)
		uint32_t irradianceSamples = 2048; // コサイン重み付きサンプル数
		float    maxSampleValue = 0.0f; // >0 なら 1 サンプルの最大成分をクランプ (ファイアフライ対策)
		uint32_t threadCount = 0;    // 0 = 自動 (CPU 版のみ)
		CubeFormat cubeFormat = CubeFormat::Float16; // Prefiltered / Irradiance の出力形式
		bool     useGpu = true;      // true (既定) なら Direct3D 12 の Compute Shader で生成 (IBLBakerGpu.cpp)
	};

	// ベイク品質のプリセット。Custom は「プリセットの値から手動で変更した状態」を表し、ApplyBakePreset では何も変えない。
	enum class BakePreset
	{
		Low,
		Medium, // BakeSettings の既定値と同じ
		High,
		Ultra,
		Custom
	};

	// プリセットが管理する項目 (サイズ / サンプル数) だけを上書きする。
	// 出力形式・処理装置・スレッド数・サンプルクランプは環境や好みの設定なので変えない。
	inline void ApplyBakePreset( BakeSettings& s, BakePreset preset )
	{
		struct Values { uint32_t src, pf, pfSamples, lut, lutSamples, irr, irrSamples; };
		Values v;
		switch( preset )
		{
		case BakePreset::Low:    v = { 256,  128, 256,   128, 256,  16,  512 };   break;
		case BakePreset::Medium: v = { 512,  256, 1024,  256, 1024, 32,  2048 };  break;
		case BakePreset::High:   v = { 1024, 512, 4096,  256, 2048, 64,  4096 };  break;
		case BakePreset::Ultra:  v = { 2048, 1024, 16384, 512, 8192, 128, 16384 }; break;
		default: return; // Custom
		}
		s.sourceCubeSize = v.src;
		s.prefilterSize = v.pf;
		s.prefilterSamples = v.pfSamples;
		s.lutSize = v.lut;
		s.lutSamples = v.lutSamples;
		s.irradianceSize = v.irr;
		s.irradianceSamples = v.irrSamples;
	}

	// プリセットが管理する項目が全て同じか (手動変更の検出用)
	inline bool SamePresetFields( const BakeSettings& a, const BakeSettings& b )
	{
		return a.sourceCubeSize == b.sourceCubeSize && a.prefilterSize == b.prefilterSize && a.prefilterSamples == b.prefilterSamples
			&& a.lutSize == b.lutSize && a.lutSamples == b.lutSamples
			&& a.irradianceSize == b.irradianceSize && a.irradianceSamples == b.irradianceSamples;
	}

	// stage は文字列リテラルを渡す。複数スレッドから呼ばれ得るのでスレッドセーフな実装にすること。
	// コールバックから BakeCancelled を投げるとベイクを中断できる (ParallelFor はワーカーの例外を呼び出し元へ伝える)。
	using ProgressFn = std::function<void( const char* stage, float progress01 )>;

	CubeMap EquirectToCube( const EquirectImage& eq, uint32_t size, uint32_t threads, const ProgressFn& progress );
	void GenerateMips( CubeMap& cube );
	CubeMap PrefilterSpecular( const CubeMap& source, const BakeSettings& s, const ProgressFn& progress );
	// 戻り値: size*size*2 の float (R=scale, G=bias)。x = NdotV, y = roughness (行0 = roughness 小)
	std::vector<float> BakeBrdfLut( uint32_t size, uint32_t samples, uint32_t threads, const ProgressFn& progress );
	// 拡散光用: 各方向 N について cosine 重み付きで半球の放射輝度を平均した値 (= irradiance(N) / π)。
	//   diffuse = albedo * 読み出した値  (π の掛け算は不要)。mip 無しの単一レベルの CubeMap を返す。
	CubeMap ConvolveIrradiance( const CubeMap& source, const BakeSettings& s, const ProgressFn& progress );
	uint32_t FloorLog2( uint32_t v );

	// 重点サンプリングのサンプル列 (タンジェント空間の方向 + 入力キューブから引く mip)。
	// CPU 版と GPU 版で同一のテーブルを使うことで、両者の結果を揃える。
	struct SampleDir
	{
		Vec3  L;   // タンジェント空間 (N = +Z)
		float mip;
	};
	// GGX 重点サンプリング (V = N)。L.z <= 0 のサンプルは除かれるので、戻り値の要素数は count 以下。L.z がそのまま NdotL。
	std::vector<SampleDir> BuildPrefilterSampleTable( float roughness, uint32_t count, uint32_t srcSize, uint32_t srcMipCount );
	// コサイン重み付きサンプリング。要素数は count ちょうど。
	std::vector<SampleDir> BuildIrradianceSampleTable( uint32_t count, uint32_t srcSize, uint32_t srcMipCount );
} // namespace ibl
