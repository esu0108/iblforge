// ExrDecode.h : OpenEXR (.exr) のデコード (TinyEXR v3)。Win32 / DirectXTex 非依存の純 C++ (ファイル I/O は呼び出し側)
#pragma once
#include <cstddef>
#include <string>

#include "IBLBaker.h"

namespace ibl
{
	// メモリ上の .exr を EquirectImage (リニア RGB) にデコードする。別スレッドから呼んでよい。
	//   最初の非 Deep パートの R/G/B チャンネルを使う。R/G/B が無ければ、輝度のみ (Y) はグレー、輝度 + 色差 (Y/RY/BY) は RGB に復元する。
	//   HALF / FLOAT / UINT のどれでもよい。NaN / Inf / 負値は 0 にする。
	//   失敗時は false と err (英語)。
	bool DecodeExr( const void* data, size_t size, EquirectImage& out, std::string& err );
} // namespace ibl
