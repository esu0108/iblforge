#include "HdrPreview.h"

#include <algorithm>
#include <cmath>

namespace ibl
{
	PreviewSource BuildPreviewSource( const EquirectImage& eq, uint32_t maxWidth )
	{
		PreviewSource p;
		const uint32_t f = std::max( 1u, ( eq.width + maxWidth - 1 ) / maxWidth );
		p.w = std::max( 1u, eq.width / f );
		p.h = std::max( 1u, eq.height / f );
		p.rgb.resize( static_cast<size_t>( p.w ) * p.h * 3 );
		double logSum = 0.0;
		for( uint32_t y = 0; y < p.h; ++y )
			for( uint32_t x = 0; x < p.w; ++x )
			{
				double r = 0, g = 0, b = 0;
				uint32_t n = 0;
				for( uint32_t dy = 0; dy < f; ++dy )
					for( uint32_t dx = 0; dx < f; ++dx )
					{
						const uint32_t sx = std::min( x * f + dx, eq.width - 1 ), sy = std::min( y * f + dy, eq.height - 1 );
						const auto& c = eq.pixels[static_cast<size_t>( sy ) * eq.width + sx];
						r += c.x; g += c.y; b += c.z;
						++n;
					}
				const float fr = static_cast<float>( r / n ), fg = static_cast<float>( g / n ), fb = static_cast<float>( b / n );
				float* o = &p.rgb[( static_cast<size_t>( y ) * p.w + x ) * 3];
				o[0] = fr; o[1] = fg; o[2] = fb;
				logSum += std::log( 0.2126 * fr + 0.7152 * fg + 0.0722 * fb + 1e-4 );
			}
		const double avg = std::exp( logSum / ( static_cast<double>( p.w ) * p.h ) );
		p.autoEV = std::clamp( static_cast<float>( std::log2( 0.18 / avg ) ), -10.0f, 10.0f );
		return p;
	}

	void TonemapPreview( const PreviewSource& p, float ev, std::vector<uint8_t>& rgba )
	{
		rgba.resize( static_cast<size_t>( p.w ) * p.h * 4 );
		const float scale = std::exp2( ev );
		auto aces = []( float x ) { return std::clamp( ( x * ( 2.51f * x + 0.03f ) ) / ( x * ( 2.43f * x + 0.59f ) + 0.14f ), 0.0f, 1.0f ); };
		for( size_t i = 0, n = static_cast<size_t>( p.w ) * p.h; i < n; ++i )
		{
			for( int c = 0; c < 3; ++c )
				rgba[i * 4 + c] = static_cast<uint8_t>( std::pow( aces( p.rgb[i * 3 + c] * scale ), 1.0f / 2.2f ) * 255.0f + 0.5f );
			rgba[i * 4 + 3] = 255;
		}
	}
} // namespace ibl
