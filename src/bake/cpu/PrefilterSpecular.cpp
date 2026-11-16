// PrefilterSpecular.cpp : CPU ベイク - GGX 重点サンプリングによる Prefiltered Specular Cube
#include "bake/IBLBaker.h"

#include <atomic>

#include "bake/cpu/CubeMath.h"
#include "bake/cpu/ParallelFor.h"

namespace ibl
{
	using namespace detail;

	CubeMap PrefilterSpecular( const CubeMap& src, const BakeSettings& s, const ProgressFn& progress )
	{
		const uint32_t P = s.prefilterSize;
		const uint32_t mipCount = FloorLog2( P ) + 1;
		const uint32_t srcSize = src.mips[0].size;

		uint64_t totalRows = 0;
		for( uint32_t m = 0; m < mipCount; ++m ) totalRows += 6ull * ( P >> m );
		std::atomic<uint64_t> doneRows{ 0 };
		auto reportRow = [&]( float ) {
			const uint64_t d = doneRows.fetch_add( 1 ) + 1;
			if( progress ) progress( "Prefilter specular", static_cast<float>( d ) / static_cast<float>( totalRows ) );
			};

		CubeMap out;
		out.mips.resize( mipCount );
		for( uint32_t m = 0; m < mipCount; ++m )
		{
			CubeMip& dst = out.mips[m];
			dst.size = P >> m;
			const uint32_t size = dst.size;
			for( auto& f : dst.face ) f.assign( static_cast<size_t>( size ) * size, Vec3{} );

			const float roughness = mipCount > 1 ? static_cast<float>( m ) / static_cast<float>( mipCount - 1 ) : 0.0f;

			if( m == 0 )
			{
				// roughness = 0 : 鏡面反射そのもの。同サイズの mip があればコピー、無ければ再サンプル
				const CubeMip* same = nullptr;
				for( const auto& sm : src.mips ) if( sm.size == size ) { same = &sm; break; }
				ParallelFor( 6 * size, s.threadCount, [&]( uint32_t row ) {
					const int f = static_cast<int>( row / size );
					const uint32_t y = row % size;
					for( uint32_t x = 0; x < size; ++x )
					{
						Vec3 c;
						if( same ) c = same->face[f][static_cast<size_t>( y ) * size + x];
						else
						{
							const float u = 2.0f * ( static_cast<float>( x ) + 0.5f ) / size - 1.0f;
							const float v = 2.0f * ( static_cast<float>( y ) + 0.5f ) / size - 1.0f;
							c = SampleMipBilinear( src.mips[0], FaceUVToDir( f, u, v ) );
						}
						dst.face[f][static_cast<size_t>( y ) * size + x] = c;
					}
				}, reportRow );
				continue;
			}

			const std::vector<SampleDir> samples = BuildPrefilterSampleTable( roughness, s.prefilterSamples, srcSize, static_cast<uint32_t>( src.mips.size() ) );
			ParallelFor( 6 * size, s.threadCount, [&]( uint32_t row ) {
				const int f = static_cast<int>( row / size );
				const uint32_t y = row % size;
				for( uint32_t x = 0; x < size; ++x )
				{
					const float u = 2.0f * ( static_cast<float>( x ) + 0.5f ) / size - 1.0f;
					const float v = 2.0f * ( static_cast<float>( y ) + 0.5f ) / size - 1.0f;
					const Vec3 N = Normalize( FaceUVToDir( f, u, v ) );
					const Vec3 up = std::fabs( N.z ) < 0.999f ? Vec3{ 0, 0, 1 } : Vec3{ 1, 0, 0 };
					const Vec3 T = Normalize( Cross( up, N ) );
					const Vec3 B = Cross( N, T );

					Vec3 sum;
					float weight = 0.0f;
					for( const SampleDir& sp : samples )
					{
						const Vec3 L = T * sp.L.x + B * sp.L.y + N * sp.L.z;
						Vec3 c = SampleCubeTrilinear( src, L, sp.mip );
						if( s.maxSampleValue > 0.0f )
						{
							const float mx = std::max( c.x, std::max( c.y, c.z ) );
							if( mx > s.maxSampleValue ) c = c * ( s.maxSampleValue / mx );
						}
						sum += c * sp.L.z; // L.z = NdotL
						weight += sp.L.z;
					}
					dst.face[f][static_cast<size_t>( y ) * size + x] =
						weight > 0.0f ? sum * ( 1.0f / weight ) : SampleMipBilinear( src.mips[0], N );
				}
			}, reportRow );
		}
		return out;
	}
} // namespace ibl
