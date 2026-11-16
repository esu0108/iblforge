#include "DescriptorTables.h"

#include <algorithm>
#include <stdexcept>

namespace ibl
{
	namespace gpu
	{
		DescriptorTables::DescriptorTables( ID3D12Device* dev ) : m_device( dev )
		{
			D3D12_DESCRIPTOR_HEAP_DESC hd{};
			hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			hd.NumDescriptors = kHeapSize;
			hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			ThrowIfFailed( m_device->CreateDescriptorHeap( &hd, IID_PPV_ARGS( &m_heap ) ), "CreateDescriptorHeap" );
			m_incr = m_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
		}

		D3D12_CPU_DESCRIPTOR_HANDLE DescriptorTables::Cpu( UINT i ) const
		{
			D3D12_CPU_DESCRIPTOR_HANDLE h = m_heap->GetCPUDescriptorHandleForHeapStart();
			h.ptr += static_cast<SIZE_T>( i ) * m_incr;
			return h;
		}
		D3D12_GPU_DESCRIPTOR_HANDLE DescriptorTables::Gpu( UINT i ) const
		{
			D3D12_GPU_DESCRIPTOR_HANDLE h = m_heap->GetGPUDescriptorHandleForHeapStart();
			h.ptr += static_cast<UINT64>( i ) * m_incr;
			return h;
		}

		UINT DescriptorTables::Alloc()
		{
			if( m_nextSlot + kTableSize > kHeapSize ) throw std::runtime_error( "GPU bake: descriptor heap exhausted" );
			const UINT t = m_nextSlot;
			m_nextSlot += kTableSize;

			D3D12_SHADER_RESOURCE_VIEW_DESC v{};
			v.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			v.Format = kFormat;
			v.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			v.Texture2D.MipLevels = 1;
			m_device->CreateShaderResourceView( nullptr, &v, Cpu( t + 0 ) );
			v.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			v.TextureCube.MipLevels = 1;
			m_device->CreateShaderResourceView( nullptr, &v, Cpu( t + 1 ) );
			v.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			v.Texture2DArray.MipLevels = 1;
			v.Texture2DArray.ArraySize = 1;
			m_device->CreateShaderResourceView( nullptr, &v, Cpu( t + 2 ) );
			SrvSamples( t, nullptr, 1 );
			return t;
		}

		void DescriptorTables::SrvEquirect( UINT t, ID3D12Resource* r )
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC v{};
			v.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			v.Format = kFormat;
			v.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			v.Texture2D.MostDetailedMip = 0;
			v.Texture2D.MipLevels = 1;
			m_device->CreateShaderResourceView( r, &v, Cpu( t + 0 ) );
		}

		void DescriptorTables::SrvCube( UINT t, ID3D12Resource* r, UINT mips )
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC v{};
			v.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			v.Format = kFormat;
			v.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			v.TextureCube.MostDetailedMip = 0;
			v.TextureCube.MipLevels = mips;
			v.TextureCube.ResourceMinLODClamp = 0.0f;
			m_device->CreateShaderResourceView( r, &v, Cpu( t + 1 ) );
		}

		void DescriptorTables::SrvMipArray( UINT t, ID3D12Resource* r, UINT mip )
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC v{};
			v.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			v.Format = kFormat;
			v.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			v.Texture2DArray.MostDetailedMip = mip;
			v.Texture2DArray.MipLevels = 1;
			v.Texture2DArray.FirstArraySlice = 0;
			v.Texture2DArray.ArraySize = 6;
			m_device->CreateShaderResourceView( r, &v, Cpu( t + 2 ) );
		}

		void DescriptorTables::SrvSamples( UINT t, ID3D12Resource* r, UINT count )
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC v{};
			v.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			v.Format = DXGI_FORMAT_UNKNOWN;
			v.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			v.Buffer.FirstElement = 0;
			v.Buffer.NumElements = std::max( count, 1u );
			v.Buffer.StructureByteStride = sizeof( Float4 );
			v.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			m_device->CreateShaderResourceView( r, &v, Cpu( t + 3 ) );
		}

		void DescriptorTables::Uav( UINT t, ID3D12Resource* r, UINT mip, UINT arraySize )
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC u{};
			u.Format = kFormat;
			u.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			u.Texture2DArray.MipSlice = mip;
			u.Texture2DArray.FirstArraySlice = 0;
			u.Texture2DArray.ArraySize = arraySize;
			u.Texture2DArray.PlaneSlice = 0;
			m_device->CreateUnorderedAccessView( r, nullptr, &u, Cpu( t + 4 ) );
		}
	} // namespace gpu
} // namespace ibl
