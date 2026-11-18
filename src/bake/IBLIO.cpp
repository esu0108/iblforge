#include "IBLIO.h"

#include <DirectXPackedVector.h>
#include <DirectXTex.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <atomic>
#include <cstring>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <tuple>

#include "Common.h"
#include "ExrDecode.h"
#include "IBLBakerGpu.h"
#include "ShFile.h"
#include "cpu/ParallelFor.h"

namespace ibl
{
	namespace
	{
		std::string Hr( const char* what, HRESULT hr ) { return std::string( what ) + " failed (HRESULT " + util::HrToString( hr ) + ")"; }

		uint32_t RoundPow2( uint32_t v, uint32_t lo, uint32_t hi )
		{
			v = std::min( std::max( v, lo ), hi );
			uint32_t p = 1u << FloorLog2( v );
			if( p != v && ( v - p ) > ( p * 2 - v ) ) p *= 2; // 近い方の2の累乗
			return std::min( std::max( p, lo ), hi );
		}

		uint16_t ToHalf( float v )
		{
			v = std::min( std::max( v, 0.0f ), 65504.0f ); // inf 化を防ぐ
			return DirectX::PackedVector::XMConvertFloatToHalf( v );
		}

		// BRDF LUT は HDRI に依存せず (サイズ, サンプル数, 計算装置) だけで決まるので、同じ条件の再ベイクでは使い回す。
		// CPU / GPU で浮動小数の結果がわずかに違い得るため、装置も鍵に含める。
		using LutKey = std::tuple<uint32_t, uint32_t, bool>;
		std::mutex g_lutMutex;
		std::map<LutKey, std::vector<float>> g_lutCache;

		bool FindCachedLut( const LutKey& k, std::vector<float>& out )
		{
			std::lock_guard<std::mutex> lock( g_lutMutex );
			const auto it = g_lutCache.find( k );
			if( it == g_lutCache.end() ) return false;
			out = it->second;
			return true;
		}
		void StoreCachedLut( const LutKey& k, const std::vector<float>& lut )
		{
			std::lock_guard<std::mutex> lock( g_lutMutex );
			if( g_lutCache.size() >= 4 ) g_lutCache.clear(); // 設定を変えながら試しても肥大しないよう小さく保つ
			g_lutCache[k] = lut;
		}
	} // namespace

	BakeOutputs BakeOutputs::ForStem( const std::filesystem::path& dir, const std::wstring& stem )
	{
		BakeOutputs o;
		o.prefilteredCube = dir / ( stem + L"_prefiltered.dds" );
		o.brdfLut = dir / ( stem + L"_brdf_lut.dds" );
		o.irradianceCube = dir / ( stem + L"_irradiance.dds" );
		o.sh = dir / ( stem + L"_sh.json" );
		return o;
	}

	// ------------------------------------------------------------------ HDR / EXR 読み込み
	namespace
	{
		// リニア RGBA float (行優先, 上が +Y, stride = 1 ピクセル 4 float) から EquirectImage を作る。NaN/Inf/負値は 0 にする
		bool FillEquirect( const float* rgba, size_t rowFloats, uint32_t width, uint32_t height, EquirectImage& out, std::string& err )
		{
			if( width < 8 || height < 4 ) { err = "HDRI is too small / invalid."; return false; }
			out.width = width;
			out.height = height;
			out.pixels.resize( static_cast<size_t>( width ) * height );
			auto sane = []( float v ) { return std::isfinite( v ) ? std::max( v, 0.0f ) : 0.0f; };
			for( uint32_t y = 0; y < height; ++y )
			{
				const float* row = rgba + static_cast<size_t>( y ) * rowFloats;
				for( uint32_t x = 0; x < width; ++x )
					out.pixels[static_cast<size_t>( y ) * width + x] = { sane( row[x * 4 + 0] ), sane( row[x * 4 + 1] ), sane( row[x * 4 + 2] ) };
			}
			return true;
		}

		bool LoadHdrFile( const std::filesystem::path& path, EquirectImage& out, std::string& err )
		{
			DirectX::TexMetadata meta{};
			DirectX::ScratchImage img;
			HRESULT hr = DirectX::LoadFromHDRFile( path.c_str(), &meta, img );
			if( FAILED( hr ) ) { err = Hr( "LoadFromHDRFile", hr ) + ": " + util::PathToUtf8( path ); return false; }

			const DirectX::ScratchImage* use = &img;
			DirectX::ScratchImage converted;
			if( meta.format != DXGI_FORMAT_R32G32B32A32_FLOAT )
			{
				hr = DirectX::Convert( img.GetImages(), img.GetImageCount(), meta, DXGI_FORMAT_R32G32B32A32_FLOAT,
									  DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted );
				if( FAILED( hr ) ) { err = Hr( "Convert(R32G32B32A32_FLOAT)", hr ); return false; }
				use = &converted;
			}

			const DirectX::Image* im = use->GetImage( 0, 0, 0 );
			if( !im ) { err = "HDRI is too small / invalid."; return false; }
			return FillEquirect( reinterpret_cast<const float*>( im->pixels ), im->rowPitch / sizeof( float ),
								 static_cast<uint32_t>( im->width ), static_cast<uint32_t>( im->height ), out, err );
		}

		// OpenEXR: ファイルは自前で (ワイド文字パスのまま) 読み、メモリ上でデコードする (TinyEXR のファイル名 API は ANSI のため)。
		bool LoadExrFile( const std::filesystem::path& path, EquirectImage& out, std::string& err )
		{
			std::ifstream f( path, std::ios::binary | std::ios::ate );
			if( !f ) { err = "Cannot open file: " + util::PathToUtf8( path ); return false; }
			const std::streamoff size = f.tellg();
			if( size <= 0 ) { err = "Empty file: " + util::PathToUtf8( path ); return false; }
			std::vector<unsigned char> bytes( static_cast<size_t>( size ) );
			f.seekg( 0 );
			if( !f.read( reinterpret_cast<char*>( bytes.data() ), size ) ) { err = "Failed to read file: " + util::PathToUtf8( path ); return false; }
			if( !DecodeExr( bytes.data(), bytes.size(), out, err ) ) { err += ": " + util::PathToUtf8( path ); return false; }
			return true;
		}
	} // namespace

	bool LoadEquirectHdr( const std::filesystem::path& path, EquirectImage& out, std::string& err )
	{
		std::wstring ext = path.extension().wstring();
		for( wchar_t& c : ext ) c = static_cast<wchar_t>( std::towlower( c ) );
		return ext == L".exr" ? LoadExrFile( path, out, err ) : LoadHdrFile( path, out, err );
	}

	// ------------------------------------------------------------------ DDS イメージ生成 / 保存
	namespace
	{
		// CubeMap を指定形式の ScratchImage (Cube, 全 mip) にする。
		//   まず RGBA32F に詰め (BC6H / R11G11B10 / FP16 の上限に収まるよう 0..65504 にクランプ)、Float32 以外は DirectXTex で変換 / 圧縮する。
		std::shared_ptr<DirectX::ScratchImage> MakeCubeImage( const CubeMap& cube, CubeFormat format, const ProgressFn& progress, const char* bc6hStage, std::string& err )
		{
			const size_t size = cube.mips[0].size;
			DirectX::ScratchImage full;
			HRESULT hr = full.InitializeCube( DXGI_FORMAT_R32G32B32A32_FLOAT, size, size, 1, cube.mips.size() );
			if( FAILED( hr ) ) { err = Hr( "ScratchImage::InitializeCube", hr ); return nullptr; }

			auto clampHdr = []( float v ) { return std::min( std::max( v, 0.0f ), 65504.0f ); }; // NaN は 0 になる
			for( size_t m = 0; m < cube.mips.size(); ++m )
				for( size_t f = 0; f < 6; ++f )
				{
					const DirectX::Image* img = full.GetImage( m, f, 0 );
					const CubeMip& src = cube.mips[m];
					for( size_t y = 0; y < src.size; ++y )
					{
						float* row = reinterpret_cast<float*>( img->pixels + y * img->rowPitch );
						for( size_t x = 0; x < src.size; ++x )
						{
							const Vec3 c = src.face[f][y * src.size + x];
							row[x * 4 + 0] = clampHdr( c.x );
							row[x * 4 + 1] = clampHdr( c.y );
							row[x * 4 + 2] = clampHdr( c.z );
							row[x * 4 + 3] = 1.0f;
						}
					}
				}

			auto out = std::make_shared<DirectX::ScratchImage>();
			if( format == CubeFormat::Float32 )
			{
				*out = std::move( full );
				return out;
			}
			if( format == CubeFormat::BC6H )
			{
				// TEX_COMPRESS_PARALLEL は DirectXTex を OpenMP 付きでビルドしたときだけ使える (無いと E_NOTIMPL)。
				// BC6H のブロック (4x4) は互いに独立なので、各画像 (mip x 面) を 32 行ごとの帯に分けて別スレッドで圧縮する。
				// 面数 (6) より細かく分けるので、コア数が多くても使い切れ、進捗も滑らかになる。
				hr = out->InitializeCube( DXGI_FORMAT_BC6H_UF16, size, size, 1, cube.mips.size() );
				if( FAILED( hr ) ) { err = Hr( "ScratchImage::InitializeCube(BC6H)", hr ); return nullptr; }
				struct Band { size_t mip, face, y0, rows; };
				constexpr size_t kBandRows = 32; // 4 の倍数 (ブロック境界に合わせる)
				std::vector<Band> bands;
				uint64_t totalPx = 0;
				for( size_t m = 0; m < cube.mips.size(); ++m )
				{
					const size_t sz = cube.mips[m].size;
					totalPx += static_cast<uint64_t>( sz ) * sz * 6;
					for( size_t f = 0; f < 6; ++f )
						for( size_t y = 0; y < sz; y += kBandRows ) bands.push_back( { m, f, y, std::min( kBandRows, sz - y ) } );
				}
				std::vector<HRESULT> results( bands.size(), S_OK );
				std::atomic<uint64_t> donePx{ 0 }; // 進捗は画素数ベース
				if( progress ) progress( bc6hStage, 0.0f );
				detail::ParallelFor( static_cast<uint32_t>( bands.size() ), 0, [&]( uint32_t t ) {
					const Band& bd = bands[t];
					const DirectX::Image* srcFull = full.GetImage( bd.mip, bd.face, 0 );
					const DirectX::Image* dst = out->GetImage( bd.mip, bd.face, 0 );
					DirectX::Image strip = *srcFull; // 元画像の y0 行目から rows 行だけを指す
					strip.height = bd.rows;
					strip.slicePitch = srcFull->rowPitch * bd.rows;
					strip.pixels = srcFull->pixels + srcFull->rowPitch * bd.y0;
					DirectX::ScratchImage one;
					HRESULT h = DirectX::Compress( strip, DXGI_FORMAT_BC6H_UF16, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, one );
					if( SUCCEEDED( h ) )
					{
						const DirectX::Image* r = one.GetImage( 0, 0, 0 );
						const size_t offset = ( bd.y0 / 4 ) * dst->rowPitch; // BC は 4 行 = 1 ブロック行
						if( r && r->rowPitch == dst->rowPitch && offset + r->slicePitch <= dst->slicePitch ) std::memcpy( dst->pixels + offset, r->pixels, r->slicePitch );
						else h = E_FAIL;
					}
					results[t] = h;
					const uint64_t px = static_cast<uint64_t>( srcFull->width ) * bd.rows;
					const uint64_t d = donePx.fetch_add( px ) + px;
					if( progress ) progress( bc6hStage, static_cast<float>( static_cast<double>( d ) / static_cast<double>( totalPx ) ) ); // BakeCancelled はここから伝わる
				} );
				for( HRESULT h : results )
					if( FAILED( h ) ) { err = Hr( "Compress(BC6H_UF16)", h ); return nullptr; }
				return out;
			}
			const DXGI_FORMAT target = format == CubeFormat::R11G11B10 ? DXGI_FORMAT_R11G11B10_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT;
			hr = DirectX::Convert( full.GetImages(), full.GetImageCount(), full.GetMetadata(), target,
								  DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, *out );
			if( FAILED( hr ) ) { err = Hr( "Convert(cube format)", hr ); return nullptr; }
			return out;
		}

		std::shared_ptr<DirectX::ScratchImage> MakeLutImage( const std::vector<float>& rg, uint32_t size, std::string& err )
		{
			auto si = std::make_shared<DirectX::ScratchImage>();
			HRESULT hr = si->Initialize2D( DXGI_FORMAT_R16G16_FLOAT, size, size, 1, 1 );
			if( FAILED( hr ) ) { err = Hr( "ScratchImage::Initialize2D", hr ); return nullptr; }

			const DirectX::Image* img = si->GetImage( 0, 0, 0 );
			for( uint32_t y = 0; y < size; ++y )
			{
				uint16_t* row = reinterpret_cast<uint16_t*>( img->pixels + static_cast<size_t>( y ) * img->rowPitch );
				for( uint32_t x = 0; x < size; ++x )
				{
					row[x * 2 + 0] = ToHalf( rg[( static_cast<size_t>( y ) * size + x ) * 2 + 0] );
					row[x * 2 + 1] = ToHalf( rg[( static_cast<size_t>( y ) * size + x ) * 2 + 1] );
				}
			}
			return si;
		}

		bool SaveDds( const DirectX::ScratchImage& si, const std::filesystem::path& path, const char* what, std::string& err )
		{
			const HRESULT hr = DirectX::SaveToDDSFile( si.GetImages(), si.GetImageCount(), si.GetMetadata(), DirectX::DDS_FLAGS_NONE, path.c_str() );
			if( FAILED( hr ) ) { err = Hr( what, hr ) + ": " + util::PathToUtf8( path ); return false; }
			return true;
		}

		bool SaveSh( const SH9& sh, const std::filesystem::path& path, std::string& err )
		{
			if( !WriteShJson( path, sh, err ) ) { err += ": " + util::PathToUtf8( path ); return false; }
			return true;
		}
	} // namespace

	bool SaveBaked( const BakedTextures& baked, const std::filesystem::path& dir, BakeOutputs& outputs, std::string& err )
	{
		if( !baked.Valid() ) { err = "There is no baked result to save."; return false; }
		std::error_code ec;
		std::filesystem::create_directories( dir, ec );
		if( ec ) { err = "Cannot create output directory: " + util::PathToUtf8( dir ); return false; }
		outputs = BakeOutputs::ForStem( dir, baked.stem );
		return SaveDds( *baked.prefiltered, outputs.prefilteredCube, "SaveToDDSFile(prefiltered)", err ) &&
			   SaveDds( *baked.lut, outputs.brdfLut, "SaveToDDSFile(lut)", err ) &&
			   SaveDds( *baked.irradiance, outputs.irradianceCube, "SaveToDDSFile(irradiance)", err ) &&
			   SaveSh( baked.sh, outputs.sh, err );
	}

	// ------------------------------------------------------------------ パイプライン
	bool RunBake( const BakeJob& job, const ProgressFn& progress, BakedTextures& out, std::string& err )
	{
		try
		{
			BakeSettings s = job.settings;
			s.sourceCubeSize = RoundPow2( s.sourceCubeSize, 64, 4096 );
			s.prefilterSize = RoundPow2( s.prefilterSize, 16, 2048 );
			s.lutSize = RoundPow2( s.lutSize, 16, 1024 );
			s.prefilterSamples = std::max( s.prefilterSamples, 16u );
			s.lutSamples = std::max( s.lutSamples, 16u );

			if( !job.hdr || job.hdr->width == 0 || job.hdr->height == 0 ) { err = "No HDRI is loaded."; return false; }
			const EquirectImage& eq = *job.hdr;

			s.irradianceSize = RoundPow2( s.irradianceSize, 8, 256 );
			s.irradianceSamples = std::max( s.irradianceSamples, 16u );

			CubeMap irradiance, prefiltered;
			std::vector<float> lut;
			SH9 sh;
			const LutKey lutKey{ s.lutSize, s.lutSamples, s.useGpu };
			const bool lutCached = FindCachedLut( lutKey, lut );
			if( s.useGpu )
			{
				// SH (L2) は HDRI から直接求める (CPU / GPU どちらのベイクでも同じ。BakeSettings のサンプルクランプも Irradiance と同様に掛ける)。
				// GPU ベイク中 CPU は待つだけなので、別スレッドで並行して計算する。結果は行ごとの総和なのでスレッド数 / 実行順によらず同じ。
				// 進捗コールバックは (ステージ名が GPU 側と入れ替わるのを避けるため) 渡さない。キャンセルは GPU 側の通知で検出し、future は下で必ず待つ。
				std::future<SH9> shFuture = std::async( std::launch::async, [&]() { return ProjectEquirectSH( eq, s.maxSampleValue, s.threadCount, ProgressFn{} ); } );

				// GPU (Direct3D 12 Compute): Equirect -> Cube -> mip -> Irradiance / Prefilter / LUT を GPU 上で実行して読み戻す
				GpuBakeResult r;
				if( lutCached ) r.brdfLut = lut; // GPU 側は計算済み LUT を見つけると LUT pass を省く
				if( !BakeOnGpu( job.device, eq, s, progress, r, err ) ) return false; // 失敗 / キャンセル時も shFuture のデストラクタが完了を待つ
				irradiance = std::move( r.irradiance );
				prefiltered = std::move( r.prefiltered );
				lut = std::move( r.brdfLut );
				sh = shFuture.get();
			}
			else
			{
				CubeMap source = EquirectToCube( eq, s.sourceCubeSize, s.threadCount, progress );
				if( progress ) progress( "Generating mips", 0.0f );
				GenerateMips( source );

				irradiance = ConvolveIrradiance( source, s, progress );
				prefiltered = PrefilterSpecular( source, s, progress );
				if( !lutCached ) lut = BakeBrdfLut( s.lutSize, s.lutSamples, s.threadCount, progress );
				// CPU ベイクは全コアを使い切るので SH は従来どおり後から計算する
				sh = ProjectEquirectSH( eq, s.maxSampleValue, s.threadCount, progress );
			}
			if( !lutCached ) StoreCachedLut( lutKey, lut );

			BakedTextures result;
			result.sh = sh;

			if( progress ) progress( "Packing textures", 0.0f );
			result.stem = job.stem;
			if( !( result.prefiltered = MakeCubeImage( prefiltered, s.cubeFormat, progress, "Compressing prefiltered (BC6H)", err ) ) ) return false;
			if( !( result.lut = MakeLutImage( lut, s.lutSize, err ) ) ) return false;
			if( !( result.irradiance = MakeCubeImage( irradiance, s.cubeFormat, progress, "Compressing irradiance (BC6H)", err ) ) ) return false;
			out = std::move( result );
			if( progress ) progress( "Done", 1.0f );
			return true;
		}
		catch( const BakeCancelled& )
		{
			err = "Canceled";
			return false;
		}
		catch( const std::exception& e )
		{
			err = std::string( "Exception: " ) + e.what();
			return false;
		}
	}
} // namespace ibl
