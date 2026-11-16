// CubeBuild.cpp : CPU ベイク - Equirect -> Cube 変換とミップ生成
#include "bake/IBLBaker.h"

#include "bake/cpu/CubeMath.h"
#include "bake/cpu/ParallelFor.h"

namespace ibl
{
	using namespace detail;

	CubeMap EquirectToCube( const EquirectImage& eq, uint32_t size, uint32_t threads, const ProgressFn& progress )
	{
		CubeMap cube;
		cube.mips.resize( 1 );
		CubeMip& m = cube.mips[0];
		m.size = size;
		for( auto& f : m.face ) f.assign( static_cast<size_t>( size ) * size, Vec3{} );

		ParallelFor( 6 * size, threads, [&]( uint32_t row ) {
			const int f = static_cast<int>( row / size );
			const uint32_t y = row % size;
			for( uint32_t x = 0; x < size; ++x )
			{
				Vec3 acc;
				for( int sy = 0; sy < 2; ++sy )
					for( int sx = 0; sx < 2; ++sx ) // 2x2 スーパーサンプリング
					{
						const float u = 2.0f * ( static_cast<float>( x ) + 0.25f + 0.5f * sx ) / size - 1.0f;
						const float v = 2.0f * ( static_cast<float>( y ) + 0.25f + 0.5f * sy ) / size - 1.0f;
						acc += SampleEquirect( eq, Normalize( FaceUVToDir( f, u, v ) ) );
					}
				m.face[f][static_cast<size_t>( y ) * size + x] = acc * 0.25f;
			}
		}, [&]( float p ) { if( progress ) progress( "Equirect -> Cube", p ); } );
		return cube;
	}

	void GenerateMips( CubeMap& cube )
	{
		while( cube.mips.back().size > 1 )
		{
			const CubeMip& src = cube.mips.back();
			CubeMip dst;
			dst.size = src.size / 2;
			for( int f = 0; f < 6; ++f )
			{
				dst.face[f].resize( static_cast<size_t>( dst.size ) * dst.size );
				for( uint32_t y = 0; y < dst.size; ++y )
					for( uint32_t x = 0; x < dst.size; ++x )
					{
						const size_t s = static_cast<size_t>( 2 * y ) * src.size + 2 * x;
						dst.face[f][static_cast<size_t>( y ) * dst.size + x] =
							( src.face[f][s] + src.face[f][s + 1] + src.face[f][s + src.size] + src.face[f][s + src.size + 1] ) * 0.25f;
					}
			}
			cube.mips.push_back( std::move( dst ) );
		}
	}
} // namespace ibl
