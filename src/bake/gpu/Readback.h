// Readback.h : GPU 上の結果テクスチャを CPU メモリ (CubeMap / float 配列) へ読み戻す
#pragma once
#include <vector>

#include "../IBLBaker.h"
#include "ComputeContext.h"

namespace ibl
{
	namespace gpu
	{
		// tex は UNORDERED_ACCESS 状態の RGBA32F キューブ (mips 枚 x 6 面)。内部で Flush する。
		CubeMap ReadbackCube( ComputeContext& ctx, ID3D12Resource* tex, UINT size, UINT mips );
		// size x size の RGBA32F テクスチャ (UNORDERED_ACCESS) から R,G だけを取り出す (size*size*2)
		std::vector<float> ReadbackLut( ComputeContext& ctx, ID3D12Resource* tex, UINT size );
	} // namespace gpu
} // namespace ibl
