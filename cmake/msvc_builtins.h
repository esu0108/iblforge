/* msvc_builtins.h : MSVC (cl.exe) 用の GCC 組み込み関数の代替。
 *   TinyEXR v3 (特に HTJ2K の exr_jph.c / exr_jph_simd.c) は __builtin_clz / __builtin_ctz などを使うが、cl.exe には無く、
 *   未宣言の関数呼び出しとしてコンパイルされて「未解決の外部シンボル __builtin_clz」のリンクエラーになる。
 *   CMakeLists.txt が tinyexr_lib の全ソースに /FI で強制インクルードする。clang-cl / GCC / Clang は組み込み関数を持つので何もしない。
 *   どれも GCC と同じく引数 0 は未定義 (呼び出し側が非 0 を保証している)。
 */
#ifndef IBLFORGE_MSVC_BUILTINS_H
#define IBLFORGE_MSVC_BUILTINS_H

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>

static __forceinline int iblforge_clz32( unsigned int v )
{
	unsigned long i;
	_BitScanReverse( &i, v );
	return 31 - (int)i;
}
static __forceinline int iblforge_ctz32( unsigned int v )
{
	unsigned long i;
	_BitScanForward( &i, v );
	return (int)i;
}
static __forceinline int iblforge_clz64( unsigned long long v )
{
	unsigned long i;
	_BitScanReverse64( &i, v );
	return 63 - (int)i;
}
static __forceinline int iblforge_ctz64( unsigned long long v )
{
	unsigned long i;
	_BitScanForward64( &i, v );
	return (int)i;
}

#define __builtin_clz( x )   iblforge_clz32( (unsigned int)( x ) )
#define __builtin_ctz( x )   iblforge_ctz32( (unsigned int)( x ) )
#define __builtin_clzll( x ) iblforge_clz64( (unsigned long long)( x ) )
#define __builtin_ctzll( x ) iblforge_ctz64( (unsigned long long)( x ) )
#endif

#endif /* IBLFORGE_MSVC_BUILTINS_H */
