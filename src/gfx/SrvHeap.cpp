#include "gfx/SrvHeap.h"

using util::ThrowIfFailed;

void SrvHeap::Init( ID3D12Device* device )
{
	D3D12_DESCRIPTOR_HEAP_DESC d{};
	d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	d.NumDescriptors = kSize;
	d.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed( device->CreateDescriptorHeap( &d, IID_PPV_ARGS( &m_heap ) ), "SRV heap" );
	m_inc = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvHeap::Cpu( UINT i ) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_heap->GetCPUDescriptorHandleForHeapStart();
	h.ptr += static_cast<SIZE_T>( i ) * m_inc;
	return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvHeap::Gpu( UINT i ) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE h = m_heap->GetGPUDescriptorHandleForHeapStart();
	h.ptr += static_cast<UINT64>( i ) * m_inc;
	return h;
}

void SrvHeap::AllocDynamic( D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu )
{
	UINT index;
	if( !m_freeDynamic.empty() )
	{
		index = m_freeDynamic.back();
		m_freeDynamic.pop_back();
	}
	else
	{
		if( m_nextDynamic >= kSize ) throw std::runtime_error( "ImGui SRV descriptors exhausted" );
		index = m_nextDynamic++;
	}
	*cpu = Cpu( index );
	*gpu = Gpu( index );
}

void SrvHeap::FreeDynamic( D3D12_CPU_DESCRIPTOR_HANDLE cpu )
{
	const SIZE_T offset = cpu.ptr - m_heap->GetCPUDescriptorHandleForHeapStart().ptr;
	const UINT index = static_cast<UINT>( offset / m_inc );
	if( index >= kReservedSlots && index < kSize ) m_freeDynamic.push_back( index );
}
