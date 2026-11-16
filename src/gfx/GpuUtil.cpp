#include "gfx/GpuUtil.h"

using util::ThrowIfFailed;

namespace gpu
{
	D3D12_RESOURCE_BARRIER Transition( ID3D12Resource* r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after )
	{
		D3D12_RESOURCE_BARRIER b{};
		b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		b.Transition.pResource = r;
		b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		b.Transition.StateBefore = before;
		b.Transition.StateAfter = after;
		return b;
	}

	D3D12_HEAP_PROPERTIES HeapProps( D3D12_HEAP_TYPE type )
	{
		D3D12_HEAP_PROPERTIES h{};
		h.Type = type;
		h.CreationNodeMask = 1;
		h.VisibleNodeMask = 1;
		return h;
	}

	D3D12_RESOURCE_DESC BufferDesc( UINT64 size )
	{
		D3D12_RESOURCE_DESC d{};
		d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		d.Width = size;
		d.Height = 1;
		d.DepthOrArraySize = 1;
		d.MipLevels = 1;
		d.Format = DXGI_FORMAT_UNKNOWN;
		d.SampleDesc.Count = 1;
		d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		return d;
	}

	ComPtr<ID3D12Resource> CreateUploadBuffer( ID3D12Device* dev, UINT64 size )
	{
		ComPtr<ID3D12Resource> r;
		const auto heap = HeapProps( D3D12_HEAP_TYPE_UPLOAD );
		const auto desc = BufferDesc( size );
		ThrowIfFailed( dev->CreateCommittedResource( &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
													 nullptr, IID_PPV_ARGS( &r ) ), "CreateCommittedResource(upload)" );
		return r;
	}
} // namespace gpu
