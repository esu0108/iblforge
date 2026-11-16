// BrdfLut.cpp : CPU ベイク - Split-Sum BRDF LUT
#include "bake/IBLBaker.h"

#include "bake/cpu/CubeMath.h"
#include "bake/cpu/ParallelFor.h"

namespace ibl
{
	using namespace detail;

	std::vector<float> BakeBrdfLut( uint32_t size, uint32_t samples, uint32_t threads, const ProgressFn& progress )
	{
		std::vector<float> lut( static_cast<size_t>( size ) * size * 2, 0.0f );
		ParallelFor( size, threads, [&]( uint32_t j ) {
			const float roughness = ( static_cast<float>( j ) + 0.5f ) / size;
			const float alpha = roughness * roughness;
			const float a2 = alpha * alpha;
			const float k = alpha * 0.5f; // IBL 用 Schlick-GGX
			for( uint32_t i = 0; i < size; ++i )
			{
				const float NdotV = std::max( ( static_cast<float>( i ) + 0.5f ) / size, 1e-3f );
				const Vec3 V = { std::sqrt( 1.0f - NdotV * NdotV ), 0.0f, NdotV };
				float A = 0.0f, B = 0.0f;
				for( uint32_t n = 0; n < samples; ++n )
				{
					const float xi1 = ( static_cast<float>( n ) + 0.5f ) / static_cast<float>( samples );
					const float xi2 = RadicalInverse( n );
					const float phi = kTwoPi * xi1;
					const float cosTheta = std::sqrt( ( 1.0f - xi2 ) / ( 1.0f + ( a2 - 1.0f ) * xi2 ) );
					const float sinTheta = std::sqrt( std::max( 0.0f, 1.0f - cosTheta * cosTheta ) );
					const Vec3 H = { sinTheta * std::cos( phi ), sinTheta * std::sin( phi ), cosTheta };
					const float VdotH = Dot( V, H );
					const Vec3 L = H * ( 2.0f * VdotH ) - V;
					const float NdotL = L.z, NdotH = H.z;
					if( NdotL > 0.0f && VdotH > 0.0f )
					{
						const float Gv = NdotV / ( NdotV * ( 1.0f - k ) + k );
						const float Gl = NdotL / ( NdotL * ( 1.0f - k ) + k );
						const float Gvis = ( Gv * Gl ) * VdotH / ( NdotH * NdotV );
						const float Fc = std::pow( 1.0f - VdotH, 5.0f );
						A += ( 1.0f - Fc ) * Gvis;
						B += Fc * Gvis;
					}
				}
				const size_t idx = ( static_cast<size_t>( j ) * size + i ) * 2;
				lut[idx + 0] = A / static_cast<float>( samples );
				lut[idx + 1] = B / static_cast<float>( samples );
			}
		}, [&]( float p ) { if( progress ) progress( "BRDF LUT", p ); } );
		return lut;
	}
} // namespace ibl
