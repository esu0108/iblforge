#include "ExrDecode.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <exr.h> // TinyEXR v3 (純 C11 の C API。C++ から使える)

namespace ibl
{
	namespace
	{
		// RAII: exr_image の解放
		struct ImageGuard
		{
			exr_image img{};
			~ImageGuard() { exr_image_free( &img ); }
		};

		const exr_channel* FindChannel( const exr_part& p, const char* name, int& index )
		{
			for( int c = 0; c < p.header.num_channels; ++c )
				if( std::strcmp( p.header.channels[c].name, name ) == 0 ) { index = c; return &p.header.channels[c]; }
			return nullptr;
		}

		// 1 チャンネルの平面 (width * height 要素, ネイティブ型) を float に広げて読む。
		// 戻り値: 要素数 w*h の float。サブサンプリングされたチャンネルは非対応 (nullptr)
		bool ReadPlane( const exr_part& p, int index, std::vector<float>& dst )
		{
			const exr_channel& ch = p.header.channels[index];
			if( ch.x_sampling != 1 || ch.y_sampling != 1 || !p.images || !p.images[index] ) return false;
			const size_t n = static_cast<size_t>( p.width ) * static_cast<size_t>( p.height );
			dst.resize( n );
			const void* src = p.images[index];
			switch( ch.pixel_type )
			{
			case EXR_PIXEL_FLOAT: std::memcpy( dst.data(), src, n * sizeof( float ) ); break;
			case EXR_PIXEL_HALF:  exr_half_to_float( static_cast<const uint16_t*>( src ), dst.data(), n ); break;
			case EXR_PIXEL_UINT:
			{
				const uint32_t* u = static_cast<const uint32_t*>( src );
				for( size_t i = 0; i < n; ++i ) dst[i] = static_cast<float>( u[i] );
				break;
			}
			default: return false;
			}
			return true;
		}

		inline float Sane( float v ) { return std::isfinite( v ) ? std::max( v, 0.0f ) : 0.0f; } // NaN/Inf/負値を除去
	} // namespace

	bool DecodeExr( const void* data, size_t size, EquirectImage& out, std::string& err )
	{
		ImageGuard g;
		const exr_result rc = exr_load_from_memory( data, size, nullptr, &g.img );
		if( !EXR_OK( rc ) ) { err = std::string( "EXR load failed: " ) + exr_result_string( rc ); return false; }

		// 色を持つ最初の非 Deep パートを探す
		const exr_part* part = nullptr;
		for( int i = 0; i < g.img.num_parts && !part; ++i )
		{
			const exr_part& p = g.img.parts[i];
			if( !p.is_deep && p.images && p.width > 0 && p.height > 0 ) part = &p;
		}
		if( !part ) { err = "EXR has no flat (non-deep) image part."; return false; }
		if( part->width < 8 || part->height < 4 ) { err = "HDRI is too small / invalid."; return false; }

		const size_t w = static_cast<size_t>( part->width ), h = static_cast<size_t>( part->height );
		out.width = static_cast<uint32_t>( w );
		out.height = static_cast<uint32_t>( h );
		out.pixels.assign( w * h, Vec3{} );

		int ir = -1, ig = -1, ib = -1, iy = -1;
		const bool hasRgb = FindChannel( *part, "R", ir ) && FindChannel( *part, "G", ig ) && FindChannel( *part, "B", ib );
		if( hasRgb )
		{
			std::vector<float> r, gch, b;
			if( !ReadPlane( *part, ir, r ) || !ReadPlane( *part, ig, gch ) || !ReadPlane( *part, ib, b ) )
			{
				err = "EXR R/G/B channels are subsampled or unreadable (not supported).";
				return false;
			}
			for( size_t i = 0; i < w * h; ++i ) out.pixels[i] = { Sane( r[i] ), Sane( gch[i] ), Sane( b[i] ) };
			return true;
		}
		if( exr_part_is_luminance_chroma( part ) ) // Y / RY / BY
		{
			float* rgba = nullptr;
			int cw = 0, ch = 0;
			const exr_result yr = exr_part_yc_to_rgba_float( nullptr, part, &rgba, &cw, &ch );
			if( !EXR_OK( yr ) || !rgba || cw != part->width || ch != part->height )
			{
				if( rgba ) std::free( rgba );
				err = std::string( "EXR luminance-chroma conversion failed: " ) + exr_result_string( yr );
				return false;
			}
			for( size_t i = 0; i < w * h; ++i ) out.pixels[i] = { Sane( rgba[i * 4 + 0] ), Sane( rgba[i * 4 + 1] ), Sane( rgba[i * 4 + 2] ) };
			std::free( rgba ); // 既定アロケータ (malloc) で確保される
			return true;
		}
		if( FindChannel( *part, "Y", iy ) ) // 輝度のみ = グレー
		{
			std::vector<float> y;
			if( !ReadPlane( *part, iy, y ) ) { err = "EXR Y channel is subsampled or unreadable (not supported)."; return false; }
			for( size_t i = 0; i < w * h; ++i ) { const float v = Sane( y[i] ); out.pixels[i] = { v, v, v }; }
			return true;
		}
		err = "EXR has no R/G/B (or Y) channels.";
		return false;
	}
} // namespace ibl
