// SphericalHarmonics.cpp : 球面調和関数 (L2 = 9 係数) の射影 / 畳み込み / 評価 (CPU。GPU ベイクでも CPU で計算する: 数百万画素の総和で一瞬)
#include "bake/IBLBaker.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "bake/cpu/CubeMath.h"
#include "bake/cpu/ParallelFor.h"

namespace ibl
{
	using namespace detail;

	void EvalSHBasis( Vec3 d, float out[9] )
	{
		out[0] = 0.282095f;
		out[1] = 0.488603f * d.y;
		out[2] = 0.488603f * d.z;
		out[3] = 0.488603f * d.x;
		out[4] = 1.092548f * d.x * d.y;
		out[5] = 1.092548f * d.y * d.z;
		out[6] = 0.315392f * ( 3.0f * d.z * d.z - 1.0f );
		out[7] = 1.092548f * d.x * d.z;
		out[8] = 0.546274f * ( d.x * d.x - d.y * d.y );
	}

	SH9 ProjectEquirectSH( const EquirectImage& eq, float maxSampleValue, uint32_t threads, const ProgressFn& progress )
	{
		const uint32_t W = eq.width, H = eq.height;
		SH9 result;
		if( W == 0 || H == 0 ) return result;

		// 列ごとの sin / cos(経度)。Equirect: u = 0.5 + atan2(x, z) / 2π なので、x = sinθ sinφ, z = sinθ cosφ (φ = (u - 0.5) * 2π)
		std::vector<float> sinPhi( W ), cosPhi( W );
		for( uint32_t i = 0; i < W; ++i )
		{
			const float phi = ( ( static_cast<float>( i ) + 0.5f ) / static_cast<float>( W ) - 0.5f ) * kTwoPi;
			sinPhi[i] = std::sin( phi );
			cosPhi[i] = std::cos( phi );
		}

		// 行ごとに倍精度で総和 -> 最後に行の順に足す (スレッド数によらず結果が同じ)
		struct RowSum { double v[9][3] = {}; };
		std::vector<RowSum> rows( H );
		const double dPhi = 2.0 * 3.14159265358979323846 / static_cast<double>( W );
		const double dTheta = 3.14159265358979323846 / static_cast<double>( H );
		ParallelFor( H, threads, [&]( uint32_t j ) {
			const float theta = ( static_cast<float>( j ) + 0.5f ) / static_cast<float>( H ) * kPi;
			const float sinT = std::sin( theta ), cosT = std::cos( theta );
			const double dOmega = dPhi * dTheta * static_cast<double>( sinT ); // この行の 1 画素の立体角
			RowSum& rs = rows[j];
			const Vec3* px = &eq.pixels[static_cast<size_t>( j ) * W];
			for( uint32_t i = 0; i < W; ++i )
			{
				Vec3 c = px[i];
				if( maxSampleValue > 0.0f )
				{
					const float mx = std::max( c.x, std::max( c.y, c.z ) );
					if( mx > maxSampleValue ) c = c * ( maxSampleValue / mx );
				}
				float y[9];
				EvalSHBasis( { sinT * sinPhi[i], cosT, sinT * cosPhi[i] }, y );
				for( int k = 0; k < 9; ++k )
				{
					const double w = static_cast<double>( y[k] ) * dOmega;
					rs.v[k][0] += w * c.x;
					rs.v[k][1] += w * c.y;
					rs.v[k][2] += w * c.z;
				}
			}
		}, [&]( float p ) { if( progress ) progress( "Spherical harmonics", p ); } );

		double sum[9][3] = {};
		for( const RowSum& rs : rows )
			for( int k = 0; k < 9; ++k )
				for( int ch = 0; ch < 3; ++ch ) sum[k][ch] += rs.v[k][ch];
		for( int k = 0; k < 9; ++k ) result.c[k] = { static_cast<float>( sum[k][0] ), static_cast<float>( sum[k][1] ), static_cast<float>( sum[k][2] ) };
		return result;
	}

	SH9 IrradianceOverPiSH( const SH9& radiance )
	{
		static const float kBand[9] = { 1.0f, 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
		SH9 out;
		for( int k = 0; k < 9; ++k ) out.c[k] = radiance.c[k] * kBand[k];
		return out;
	}

	Vec3 EvalSH( const SH9& sh, Vec3 dir )
	{
		float y[9];
		EvalSHBasis( dir, y );
		Vec3 r;
		for( int k = 0; k < 9; ++k ) r += sh.c[k] * y[k];
		return r;
	}
} // namespace ibl
