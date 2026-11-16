// DescriptorTables.h : シェーダーから見える CBV/SRV/UAV ヒープと、1 pass 分 (連続 5 個) のテーブル管理。
//   テーブル = t0: 2D SRV (equirect) / t1: Cube SRV / t2: 2DArray SRV (mip) / t3: StructuredBuffer SRV (サンプル) / u0: UAV
#pragma once
#include "GpuCommon.h"

namespace ibl
{
	namespace gpu
	{
		class DescriptorTables
		{
		public:
			explicit DescriptorTables( ID3D12Device* dev );

			ID3D12DescriptorHeap* Heap() const { return m_heap.Get(); }
			D3D12_GPU_DESCRIPTOR_HANDLE Gpu( UINT table ) const;

			// 新しいテーブルを確保する。未使用 SRV は null で埋める (Resource Binding Tier 1 対策)
			UINT Alloc();

			void SrvEquirect( UINT table, ID3D12Resource* r );
			void SrvCube( UINT table, ID3D12Resource* r, UINT mips );
			void SrvMipArray( UINT table, ID3D12Resource* r, UINT mip );
			void SrvSamples( UINT table, ID3D12Resource* r, UINT count );
			void Uav( UINT table, ID3D12Resource* r, UINT mip, UINT arraySize );

		private:
			D3D12_CPU_DESCRIPTOR_HANDLE Cpu( UINT i ) const;

			ID3D12Device* m_device = nullptr;
			ComPtr<ID3D12DescriptorHeap> m_heap;
			UINT m_incr = 0;
			UINT m_nextSlot = 0;
		};
	} // namespace gpu
} // namespace ibl
