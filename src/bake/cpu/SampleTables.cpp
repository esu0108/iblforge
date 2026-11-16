// SampleTables.cpp : 重点サンプリングのサンプル列 (CPU 版と GPU 版で共有) と FloorLog2
#include "bake/IBLBaker.h"

#include <algorithm>

#include "bake/cpu/CubeMath.h"

namespace ibl
{
	using namespace detail;

	namespace
	{
		struct PrefilterSample
		{
			Vec3  L;    // タンジェント空間 (N = V = +Z)
			float ndotl;
			float mip;  // 入力キューブから引く mip (pdf ベースのフィルタ付き重点サンプリング)
		};

		std::vector<PrefilterSample> BuildPrefilterSamples( float roughness, uint32_t count, float saTexel, float maxMip )
		{
			std::vector<PrefilterSample> out;
			out.reserve( count );
			const float alpha = roughness * roughness;
			const float a2 = alpha * alpha;
			for( uint32_t i = 0; i < count; ++i )
			{
				const float xi1 = ( static_cast<float>( i ) + 0.5f ) / static_cast<float>( count );
				const float xi2 = RadicalInverse( i );
				const float phi = kTwoPi * xi1;
				const float cosTheta = std::sqrt( ( 1.0f - xi2 ) / ( 1.0f + ( a2 - 1.0f ) * xi2 ) );
				const float sinTheta = std::sqrt( std::max( 0.0f, 1.0f - cosTheta * cosTheta ) );
				const Vec3 H = { sinTheta * std::cos( phi ), sinTheta * std::sin( phi ), cosTheta };
				// V = N = (0,0,1)  =>  L = 2 (V·H) H - V
				const Vec3 L = { 2.0f * cosTheta * H.x, 2.0f * cosTheta * H.y, 2.0f * cosTheta * cosTheta - 1.0f };
				if( L.z <= 0.0f ) continue;

				const float d = cosTheta * cosTheta * ( a2 - 1.0f ) + 1.0f;
				const float D = a2 / ( kPi * d * d );
				const float pdf = D * 0.25f; // D * NdotH / (4 * VdotH),  N = V => NdotH == VdotH
				const float saSample = 1.0f / ( static_cast<float>( count ) * pdf + 1e-6f );
				float mip = 0.5f * std::log2( saSample / saTexel ) + 1.0f;
				mip = std::min( std::max( mip, 0.0f ), maxMip );
				out.push_back( { L, L.z, mip } );
			}
			return out;
		}
	} // namespace

	uint32_t FloorLog2( uint32_t v )
	{
		uint32_t r = 0;
		while( ( 1u << ( r + 1 ) ) <= v ) ++r;
		return r;
	}

	std::vector<SampleDir> BuildPrefilterSampleTable( float roughness, uint32_t count, uint32_t srcSize, uint32_t srcMipCount )
	{
		const float saTexel = 4.0f * kPi / ( 6.0f * static_cast<float>( srcSize ) * srcSize );
		const std::vector<PrefilterSample> in = BuildPrefilterSamples( roughness, count, saTexel, static_cast<float>( srcMipCount - 1 ) );
		std::vector<SampleDir> out;
		out.reserve( in.size() );
		for( const PrefilterSample& p : in ) out.push_back( { p.L, p.mip } );
		return out;
	}

	std::vector<SampleDir> BuildIrradianceSampleTable( uint32_t count, uint32_t srcSize, uint32_t srcMipCount )
	{
		const float saTexel = 4.0f * kPi / ( 6.0f * static_cast<float>( srcSize ) * srcSize );
		const float maxSrcMip = static_cast<float>( srcMipCount - 1 );
		// pdf = cosθ/π
		std::vector<SampleDir> samples( count );
		for( uint32_t i = 0; i < count; ++i )
		{
			const float xi1 = ( static_cast<float>( i ) + 0.5f ) / static_cast<float>( count );
			const float xi2 = RadicalInverse( i );
			const float phi = kTwoPi * xi1;
			const float cosT = std::sqrt( 1.0f - xi2 ), sinT = std::sqrt( xi2 );
			const float pdf = std::max( cosT, 1e-4f ) / kPi;
			const float saSample = 1.0f / ( static_cast<float>( count ) * pdf );
			float mip = 0.5f * std::log2( saSample / saTexel ) + 1.0f;
			mip = std::min( std::max( mip, 0.0f ), maxSrcMip );
			samples[i] = { { sinT * std::cos( phi ), sinT * std::sin( phi ), cosT }, mip };
		}
		return samples;
	}
} // namespace ibl
