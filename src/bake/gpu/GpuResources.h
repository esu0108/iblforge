// GpuResources.h : GPU ベイクで使うテクスチャ / バッファの生成と、CPU メモリからのアップロード
#pragma once
#include <vector>

#include "../IBLBaker.h"
#include "ComputeContext.h"

namespace ibl
{
	namespace gpu
	{
		ComPtr<ID3D12Resource> CreateTexture( ID3D12Device* dev, UINT size, UINT arraySize, UINT mips, bool uav, D3D12_RESOURCE_STATES state );
		ComPtr<ID3D12Resource> CreateBuffer( ID3D12Device* dev, UINT64 size, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state );

		// 等距円筒 HDRI を RGBA32F テクスチャにアップロードする (完了時は NON_PIXEL_SHADER_RESOURCE。最後のバリアはリストに積んだだけ)
		ComPtr<ID3D12Resource> UploadEquirect( ComputeContext& ctx, const EquirectImage& eq );
		// サンプル方向テーブルを StructuredBuffer (NON_PIXEL_SHADER_RESOURCE) にアップロードする
		ComPtr<ID3D12Resource> UploadSamples( ComputeContext& ctx, const std::vector<Float4>& data );
		// キューブの指定 mip の 6 面を一括で遷移させる (サブリソース = mip + slice * mipCount)
		void TransitionAllMipSlices( ID3D12GraphicsCommandList* cl, ID3D12Resource* r, UINT mip, UINT mipCount,
									 D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after );
	} // namespace gpu
} // namespace ibl
