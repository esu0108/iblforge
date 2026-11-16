// SampleTableBuffer.h : CPU 版と同一のサンプル方向テーブル (irradiance + 各 prefilter mip) を 1 本の配列に連結する
#pragma once
#include <vector>

#include "../IBLBaker.h"
#include "GpuCommon.h"

namespace ibl
{
	namespace gpu
	{
		struct SampleTableBuffer
		{
			std::vector<Float4> data;           // xyz = 方向 L, w = 参照 mip
			uint32_t irrCount = 0;
			uint32_t irrOffset = 0;
			std::vector<uint32_t> pfOffset;     // mip ごとの先頭 / 個数 (mip 0 は未使用)
			std::vector<uint32_t> pfCount;

			static SampleTableBuffer Build( const BakeSettings& s, UINT srcSize, UINT srcMips, UINT pfMips );
		};
	} // namespace gpu
} // namespace ibl
