#include "gfx/TextureUpload.h"

#include <DirectXTex.h>

#include <cstring>
#include <vector>

using util::ThrowIfFailed;
using namespace gpu;
using namespace DirectX;

ComPtr<ID3D12Resource> UploadTexture( GpuContext& gpuCtx, const ScratchImage& scratch )
{
	ID3D12Device* dev = gpuCtx.Device();
	const TexMetadata& md = scratch.GetMetadata();

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = static_cast<UINT64>( md.width );
	desc.Height = static_cast<UINT>( md.height );
	desc.DepthOrArraySize = static_cast<UINT16>( md.arraySize );
	desc.MipLevels = static_cast<UINT16>( md.mipLevels );
	desc.Format = md.format;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	ComPtr<ID3D12Resource> tex;
	const auto heap = HeapProps( D3D12_HEAP_TYPE_DEFAULT );
	ThrowIfFailed( dev->CreateCommittedResource( &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS( &tex ) ), "Create texture" );

	const UINT numSub = static_cast<UINT>( md.mipLevels * md.arraySize );
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts( numSub );
	std::vector<UINT> numRows( numSub );
	std::vector<UINT64> rowSizes( numSub );
	UINT64 total = 0;
	dev->GetCopyableFootprints( &desc, 0, numSub, 0, layouts.data(), numRows.data(), rowSizes.data(), &total );

	ComPtr<ID3D12Resource> upload = CreateUploadBuffer( dev, total );
	UINT8* dst = nullptr;
	ThrowIfFailed( upload->Map( 0, nullptr, reinterpret_cast<void**>( &dst ) ), "Map(upload)" );
	for( UINT s = 0; s < numSub; ++s )
	{
		const size_t mip = s % md.mipLevels;     // D3D12 のサブリソース順: mip + array * mipLevels
		const size_t item = s / md.mipLevels;
		const Image* img = scratch.GetImage( mip, item, 0 );
		if( !img ) throw std::runtime_error( "Missing image in DDS" );
		for( UINT row = 0; row < numRows[s]; ++row )
			std::memcpy( dst + layouts[s].Offset + static_cast<size_t>( row ) * layouts[s].Footprint.RowPitch,
						img->pixels + static_cast<size_t>( row ) * img->rowPitch, static_cast<size_t>( rowSizes[s] ) );
	}
	upload->Unmap( 0, nullptr );

	ComPtr<ID3D12CommandAllocator> alloc;
	ComPtr<ID3D12GraphicsCommandList> cl;
	ThrowIfFailed( dev->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &alloc ) ), "upload allocator" );
	ThrowIfFailed( dev->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS( &cl ) ), "upload list" );
	for( UINT s = 0; s < numSub; ++s )
	{
		D3D12_TEXTURE_COPY_LOCATION d{}, sLoc{};
		d.pResource = tex.Get();
		d.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		d.SubresourceIndex = s;
		sLoc.pResource = upload.Get();
		sLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		sLoc.PlacedFootprint = layouts[s];
		cl->CopyTextureRegion( &d, 0, 0, 0, &sLoc, nullptr );
	}
	const auto barrier = Transition( tex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
	cl->ResourceBarrier( 1, &barrier );
	ThrowIfFailed( cl->Close(), "upload close" );
	ID3D12CommandList* lists[] = { cl.Get() };
	gpuCtx.Queue()->ExecuteCommandLists( 1, lists );
	gpuCtx.WaitIdle(); // upload / alloc が生きている間に完了させる
	return tex;
}
