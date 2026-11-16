// BakePipelines.h : ibl_bake.hlsl のルートシグネチャと 6 本のコンピュート PSO (ビルド時にコンパイル済みのシェーダーを読み込む)
#pragma once
#include "GpuCommon.h"

namespace ibl
{
	namespace gpu
	{
		struct BakePipelines
		{
			explicit BakePipelines( ID3D12Device* dev ); // 失敗時 std::runtime_error (コンパイルエラー本文入り)

			ComPtr<ID3D12RootSignature> rootSig;
			ComPtr<ID3D12PipelineState> equirect, downsample, copy, prefilter, irradiance, lut;
		};
	} // namespace gpu
} // namespace ibl
