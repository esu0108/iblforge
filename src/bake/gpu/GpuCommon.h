// GpuCommon.h : GPU ベイク (bake/gpu/*) 共通の定数・小物
#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

#include "Common.h"

namespace ibl
{
	namespace gpu
	{
		using Microsoft::WRL::ComPtr;
		using util::ThrowIfFailed;

		// 中間・出力テクスチャはすべて RGBA32F (typed UAV の書き込みが FL11 で保証される形式 + CPU 版と同精度)
		constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
		constexpr UINT kGroup = 8;     // numthreads(8,8,1)
		constexpr UINT kTableSize = 5; // 1 dispatch あたりのディスクリプタ: SRV x4 + UAV x1
		constexpr UINT kHeapSize = 512;
		// 1 回の ExecuteCommandLists にまとめるサンプル反復数の上限。
		// 重い dispatch を 1 回で投げると Windows の TDR (既定 2 秒) に引っかかるため、行バンドに分割して都度 GPU 完了を待つ。
		constexpr uint64_t kWorkBudget = 128ull * 1000 * 1000;

		struct GpuParams // ibl_bake.hlsl の cbuffer Params と一致させる (12 DWORD)
		{
			uint32_t size = 0;
			uint32_t rowOffset = 0;
			uint32_t sampleCount = 0;
			uint32_t sampleOffset = 0;
			uint32_t eqWidth = 0;
			uint32_t eqHeight = 0;
			uint32_t pad0 = 0, pad1 = 0;
			float    roughness = 0.0f;
			float    maxSample = 0.0f;
			float    pad2 = 0.0f, pad3 = 0.0f;
		};
		static_assert( sizeof( GpuParams ) == 12 * 4, "root constants layout" );

		struct Float4 { float x, y, z, w; };

		inline uint32_t AlignUp( uint32_t v, uint32_t a ) { return ( v + a - 1 ) / a * a; }
		inline uint32_t DivUp( uint32_t v, uint32_t a ) { return ( v + a - 1 ) / a; }

		inline D3D12_HEAP_PROPERTIES HeapProps( D3D12_HEAP_TYPE t )
		{
			D3D12_HEAP_PROPERTIES h{};
			h.Type = t;
			h.CreationNodeMask = 1;
			h.VisibleNodeMask = 1;
			return h;
		}

		inline D3D12_RESOURCE_BARRIER TransitionBarrier( ID3D12Resource* r, UINT subresource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after )
		{
			D3D12_RESOURCE_BARRIER b{};
			b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			b.Transition.pResource = r;
			b.Transition.Subresource = subresource;
			b.Transition.StateBefore = before;
			b.Transition.StateAfter = after;
			return b;
		}
	} // namespace gpu
} // namespace ibl
