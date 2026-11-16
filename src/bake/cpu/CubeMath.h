// CubeMath.h : CPU ベイクの内部で使うベクトル演算 / キューブ座標 / サンプリング (ヘッダのみ。公開 API は IBLBaker.h)
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "bake/IBLBaker.h"

namespace ibl::detail
{
inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

// ---------------------------------------------------------------- ベクトル
inline float Dot( Vec3 a, Vec3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross( Vec3 a, Vec3 b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline Vec3 Normalize( Vec3 v )
{
	const float l = std::sqrt( Dot( v, v ) );
	return l > 0.0f ? v * ( 1.0f / l ) : Vec3{ 0, 1, 0 };
}
inline Vec3 Lerp( Vec3 a, Vec3 b, float t ) { return a * ( 1.0f - t ) + b * t; }
inline float Clamp01( float v ) { return std::min( 1.0f, std::max( 0.0f, v ) ); }

// ---------------------------------------------------------------- キューブ座標
// u,v は [-1,1]。v は画像の下方向が +。D3D の TextureCube と同じ割り当て。
inline Vec3 FaceUVToDir( int face, float u, float v )
{
	switch( face )
	{
	case 0:  return { 1.0f, -v, -u };   // +X
	case 1:  return { -1.0f, -v, u };   // -X
	case 2:  return { u, 1.0f, v };     // +Y
	case 3:  return { u, -1.0f, -v };   // -Y
	case 4:  return { u, -v, 1.0f };    // +Z
	default: return { -u, -v, -1.0f };  // -Z
	}
}

// 出力 u,v は [0,1]
inline void DirToFaceUV( Vec3 d, int& face, float& u, float& v )
{
	const float ax = std::fabs( d.x ), ay = std::fabs( d.y ), az = std::fabs( d.z );
	float sc, tc, ma;
	if( ax >= ay && ax >= az )
	{
		ma = ax;
		if( d.x > 0 ) { face = 0; sc = -d.z; tc = -d.y; }
		else         { face = 1; sc = d.z;  tc = -d.y; }
	}
	else if( ay >= az )
	{
		ma = ay;
		if( d.y > 0 ) { face = 2; sc = d.x; tc = d.z; }
		else         { face = 3; sc = d.x; tc = -d.z; }
	}
	else
	{
		ma = az;
		if( d.z > 0 ) { face = 4; sc = d.x;  tc = -d.y; }
		else         { face = 5; sc = -d.x; tc = -d.y; }
	}
	ma = std::max( ma, 1e-20f );
	u = 0.5f * ( sc / ma + 1.0f );
	v = 0.5f * ( tc / ma + 1.0f );
}

// 面の外にはみ出した texel は隣の面を引き直す (シームレス)
inline Vec3 FetchTexel( const CubeMip& m, int face, int ix, int iy )
{
	const int n = static_cast<int>( m.size );
	if( ix >= 0 && ix < n && iy >= 0 && iy < n ) return m.face[face][static_cast<size_t>( iy ) * n + ix];

	const float u = ( static_cast<float>( ix ) + 0.5f ) / n * 2.0f - 1.0f;
	const float v = ( static_cast<float>( iy ) + 0.5f ) / n * 2.0f - 1.0f;
	int f2; float u2, v2;
	DirToFaceUV( FaceUVToDir( face, u, v ), f2, u2, v2 );
	const int x2 = std::min( n - 1, std::max( 0, static_cast<int>( u2 * n ) ) );
	const int y2 = std::min( n - 1, std::max( 0, static_cast<int>( v2 * n ) ) );
	return m.face[f2][static_cast<size_t>( y2 ) * n + x2];
}

inline Vec3 SampleMipBilinear( const CubeMip& m, Vec3 dir )
{
	int f; float u, v;
	DirToFaceUV( dir, f, u, v );
	const float n = static_cast<float>( m.size );
	const float fx = u * n - 0.5f, fy = v * n - 0.5f;
	const int x0 = static_cast<int>( std::floor( fx ) ), y0 = static_cast<int>( std::floor( fy ) );
	const float tx = fx - x0, ty = fy - y0;
	const Vec3 c00 = FetchTexel( m, f, x0, y0 ), c10 = FetchTexel( m, f, x0 + 1, y0 );
	const Vec3 c01 = FetchTexel( m, f, x0, y0 + 1 ), c11 = FetchTexel( m, f, x0 + 1, y0 + 1 );
	return Lerp( Lerp( c00, c10, tx ), Lerp( c01, c11, tx ), ty );
}

inline Vec3 SampleCubeTrilinear( const CubeMap& c, Vec3 dir, float mip )
{
	const int last = static_cast<int>( c.mips.size() ) - 1;
	mip = std::min( std::max( mip, 0.0f ), static_cast<float>( last ) );
	const int m0 = static_cast<int>( std::floor( mip ) );
	const int m1 = std::min( m0 + 1, last );
	const float t = mip - m0;
	const Vec3 a = SampleMipBilinear( c.mips[m0], dir );
	if( m1 == m0 || t <= 0.0f ) return a;
	return Lerp( a, SampleMipBilinear( c.mips[m1], dir ), t );
}

// ---------------------------------------------------------------- Equirect
inline Vec3 SampleEquirect( const EquirectImage& e, Vec3 d )
{
	const float u = 0.5f + std::atan2( d.x, d.z ) * ( 1.0f / kTwoPi );
	const float v = std::acos( std::min( 1.0f, std::max( -1.0f, d.y ) ) ) * ( 1.0f / kPi );
	const float fx = u * e.width - 0.5f, fy = v * e.height - 0.5f;
	const int x0 = static_cast<int>( std::floor( fx ) ), y0 = static_cast<int>( std::floor( fy ) );
	const float tx = fx - x0, ty = fy - y0;
	const int w = static_cast<int>( e.width ), h = static_cast<int>( e.height );
	auto px = [&]( int x, int y ) -> Vec3 {
		x = ( ( x % w ) + w ) % w;            // 経度はラップ
		y = std::min( h - 1, std::max( 0, y ) ); // 緯度はクランプ
		return e.pixels[static_cast<size_t>( y ) * w + x];
		};
	return Lerp( Lerp( px( x0, y0 ), px( x0 + 1, y0 ), tx ), Lerp( px( x0, y0 + 1 ), px( x0 + 1, y0 + 1 ), tx ), ty );
}

// ---------------------------------------------------------------- サンプリング
inline float RadicalInverse( uint32_t bits )
{
	bits = ( bits << 16u ) | ( bits >> 16u );
	bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
	bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
	bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
	bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
	return static_cast<float>( bits ) * 2.3283064365386963e-10f;
}
} // namespace ibl::detail
