#include "Readback.h"

#include <algorithm>

#include "GpuResources.h"

namespace ibl
{
	namespace gpu
	{
		CubeMap ReadbackCube( ComputeContext& ctx, ID3D12Resource* tex, UINT size, UINT mips )
		{
			ID3D12GraphicsCommandList* cl = ctx.List();
			const D3D12_RESOURCE_DESC desc = tex->GetDesc();
			const UINT numSub = mips * 6;
			std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts( numSub );
			std::vector<UINT> numRows( numSub );
			std::vector<UINT64> rowSizes( numSub );
			UINT64 total = 0;
			ctx.Device()->GetCopyableFootprints( &desc, 0, numSub, 0, layouts.data(), numRows.data(), rowSizes.data(), &total );

			ComPtr<ID3D12Resource> rb = CreateBuffer( ctx.Device(), total, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST );
			const auto toSrc = TransitionBarrier( tex, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE );
			cl->ResourceBarrier( 1, &toSrc );
			for( UINT s = 0; s < numSub; ++s )
			{
				D3D12_TEXTURE_COPY_LOCATION dl{}, sl{};
				dl.pResource = rb.Get();
				dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				dl.PlacedFootprint = layouts[s];
				sl.pResource = tex;
				sl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				sl.SubresourceIndex = s;
				cl->CopyTextureRegion( &dl, 0, 0, 0, &sl, nullptr );
			}
			ctx.Flush();

			const UINT8* data = nullptr;
			const D3D12_RANGE readRange = { 0, static_cast<SIZE_T>( total ) };
			ThrowIfFailed( rb->Map( 0, &readRange, reinterpret_cast<void**>( const_cast<UINT8**>( &data ) ) ), "Map(readback)" );

			CubeMap cube;
			cube.mips.resize( mips );
			for( UINT m = 0; m < mips; ++m )
			{
				CubeMip& dst = cube.mips[m];
				dst.size = std::max( size >> m, 1u );
				for( UINT f = 0; f < 6; ++f )
				{
					const UINT s = m + f * mips;
					dst.face[f].resize( static_cast<size_t>( dst.size ) * dst.size );
					for( UINT y = 0; y < dst.size; ++y )
					{
						const float* row = reinterpret_cast<const float*>( data + layouts[s].Offset + static_cast<size_t>( y ) * layouts[s].Footprint.RowPitch );
						for( UINT x = 0; x < dst.size; ++x )
							dst.face[f][static_cast<size_t>( y ) * dst.size + x] = { row[x * 4 + 0], row[x * 4 + 1], row[x * 4 + 2] };
					}
				}
			}
			const D3D12_RANGE none = { 0, 0 };
			rb->Unmap( 0, &none );
			return cube;
		}

		std::vector<float> ReadbackLut( ComputeContext& ctx, ID3D12Resource* tex, UINT size )
		{
			ID3D12GraphicsCommandList* cl = ctx.List();
			const D3D12_RESOURCE_DESC desc = tex->GetDesc();
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
			UINT rows = 0;
			UINT64 rowSize = 0, total = 0;
			ctx.Device()->GetCopyableFootprints( &desc, 0, 1, 0, &layout, &rows, &rowSize, &total );

			ComPtr<ID3D12Resource> rb = CreateBuffer( ctx.Device(), total, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST );
			const auto toSrc = TransitionBarrier( tex, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE );
			cl->ResourceBarrier( 1, &toSrc );
			D3D12_TEXTURE_COPY_LOCATION dl{}, sl{};
			dl.pResource = rb.Get();
			dl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			dl.PlacedFootprint = layout;
			sl.pResource = tex;
			sl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			sl.SubresourceIndex = 0;
			cl->CopyTextureRegion( &dl, 0, 0, 0, &sl, nullptr );
			ctx.Flush();

			const UINT8* data = nullptr;
			const D3D12_RANGE readRange = { 0, static_cast<SIZE_T>( total ) };
			ThrowIfFailed( rb->Map( 0, &readRange, reinterpret_cast<void**>( const_cast<UINT8**>( &data ) ) ), "Map(readback LUT)" );
			std::vector<float> lut( static_cast<size_t>( size ) * size * 2 );
			for( UINT y = 0; y < size; ++y )
			{
				const float* row = reinterpret_cast<const float*>( data + layout.Offset + static_cast<size_t>( y ) * layout.Footprint.RowPitch );
				for( UINT x = 0; x < size; ++x )
				{
					lut[( static_cast<size_t>( y ) * size + x ) * 2 + 0] = row[x * 4 + 0];
					lut[( static_cast<size_t>( y ) * size + x ) * 2 + 1] = row[x * 4 + 1];
				}
			}
			const D3D12_RANGE none = { 0, 0 };
			rb->Unmap( 0, &none );
			return lut;
		}
	} // namespace gpu
} // namespace ibl
