// Irradiance.cpp : CPU ベイク - 拡散光用 Irradiance Cube (コサイン畳み込み)
#include "bake/IBLBaker.h"

#include "bake/cpu/CubeMath.h"
#include "bake/cpu/ParallelFor.h"

namespace ibl
{
	using namespace detail;

	CubeMap ConvolveIrradiance( const CubeMap& src, const BakeSettings& s, const ProgressFn& progress )
	{
		const uint32_t size = s.irradianceSize;
		const uint32_t count = std::max( s.irradianceSamples, 16u );

		// コサイン重み付きサンプル。方向は N 非依存なので 1 回だけ生成して全 texel で使い回す。
		const std::vector<SampleDir> samples = BuildIrradianceSampleTable( count, src.mips[0].size, static_cast<uint32_t>( src.mips.size() ) );

		CubeMap out;
		out.mips.resize( 1 );
		CubeMip& dst = out.mips[0];
		dst.size = size;
		for( auto& f : dst.face ) f.assign( static_cast<size_t>( size ) * size, Vec3{} );

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
				for( const SampleDir& sp : samples )
				{
					Vec3 c = SampleCubeTrilinear( src, T * sp.L.x + B * sp.L.y + N * sp.L.z, sp.mip );
					if( s.maxSampleValue > 0.0f )
					{
						const float mx = std::max( c.x, std::max( c.y, c.z ) );
						if( mx > s.maxSampleValue ) c = c * ( s.maxSampleValue / mx );
					}
					sum += c;
				}
				dst.face[f][static_cast<size_t>( y ) * size + x] = sum * ( 1.0f / static_cast<float>( count ) );
			}
		}, [&]( float p ) { if( progress ) progress( "Irradiance (diffuse)", p ); } );
		return out;
	}
} // namespace ibl
