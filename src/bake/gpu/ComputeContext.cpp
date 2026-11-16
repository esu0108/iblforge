#include "ComputeContext.h"

#include <dxgi1_6.h>

#include <stdexcept>

namespace ibl
{
	namespace gpu
	{
		void ComputeContext::CreateOwnDevice()
		{
			ComPtr<IDXGIFactory4> factory;
			ThrowIfFailed( CreateDXGIFactory2( 0, IID_PPV_ARGS( &factory ) ), "CreateDXGIFactory2" );

			// ビューア (Renderer) と同じく高性能 GPU を優先
			ComPtr<IDXGIAdapter1> adapter;
			ComPtr<IDXGIFactory6> factory6;
			if( SUCCEEDED( factory.As( &factory6 ) ) )
			{
				for( UINT i = 0; SUCCEEDED( factory6->EnumAdapterByGpuPreference( i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS( &adapter ) ) ); ++i )
				{
					DXGI_ADAPTER_DESC1 d{};
					adapter->GetDesc1( &d );
					if( !( d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) &&
						SUCCEEDED( D3D12CreateDevice( adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof( ID3D12Device ), nullptr ) ) )
						break;
					adapter.Reset();
				}
			}
			ThrowIfFailed( D3D12CreateDevice( adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( &m_device ) ), "D3D12CreateDevice (no Direct3D 12 GPU?)" );
		}

		ComputeContext::ComputeContext( ID3D12Device* device ) : m_device( device )
		{
			if( !m_device ) CreateOwnDevice();

			// RGBA32F をテクスチャとしてフィルタ付きサンプル + typed UAV 書き込みできること (FL11 以上の GPU なら通常満たす)
			D3D12_FEATURE_DATA_FORMAT_SUPPORT fs{};
			fs.Format = kFormat;
			ThrowIfFailed( m_device->CheckFeatureSupport( D3D12_FEATURE_FORMAT_SUPPORT, &fs, sizeof( fs ) ), "CheckFeatureSupport(FORMAT_SUPPORT)" );
			if( !( fs.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE ) || !( fs.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE ) )
				throw std::runtime_error( "This GPU does not support R32G32B32A32_FLOAT sampling / typed UAV store." );

			D3D12_COMMAND_QUEUE_DESC qd{};
			qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			ThrowIfFailed( m_device->CreateCommandQueue( &qd, IID_PPV_ARGS( &m_queue ) ), "CreateCommandQueue" );
			ThrowIfFailed( m_device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &m_alloc ) ), "CreateCommandAllocator" );
			ThrowIfFailed( m_device->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_alloc.Get(), nullptr, IID_PPV_ARGS( &m_cl ) ), "CreateCommandList" );
			ThrowIfFailed( m_device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &m_fence ) ), "CreateFence" );
			m_event = CreateEventW( nullptr, FALSE, FALSE, nullptr );
		}

		ComputeContext::~ComputeContext()
		{
			// 共有デバイスでは、実行中 (または中断時に投入済み) のコマンドが残ったまま破棄しない
			if( m_queue && m_fence && m_event )
			{
				const UINT64 v = ++m_fenceValue;
				if( SUCCEEDED( m_queue->Signal( m_fence.Get(), v ) ) && m_fence->GetCompletedValue() < v &&
					SUCCEEDED( m_fence->SetEventOnCompletion( v, m_event ) ) )
					WaitForSingleObject( m_event, INFINITE );
			}
			if( m_event ) CloseHandle( m_event );
		}

		void ComputeContext::Flush()
		{
			ThrowIfFailed( m_cl->Close(), "CommandList::Close" );
			ID3D12CommandList* lists[] = { m_cl.Get() };
			m_queue->ExecuteCommandLists( 1, lists );
			const UINT64 v = ++m_fenceValue;
			ThrowIfFailed( m_queue->Signal( m_fence.Get(), v ), "Signal" );
			if( m_fence->GetCompletedValue() < v )
			{
				ThrowIfFailed( m_fence->SetEventOnCompletion( v, m_event ), "SetEventOnCompletion" );
				WaitForSingleObject( m_event, INFINITE );
			}
			const HRESULT removed = m_device->GetDeviceRemovedReason();
			if( FAILED( removed ) )
				throw std::runtime_error( "GPU device was removed during baking (driver reset / timeout?) (HRESULT " + util::HrToString( removed ) +
										  "). Lower the sample count or the sizes, or use the CPU mode." );
			ThrowIfFailed( m_alloc->Reset(), "CommandAllocator::Reset" );
			ThrowIfFailed( m_cl->Reset( m_alloc.Get(), nullptr ), "CommandList::Reset" );
			m_pending = 0;
		}
	} // namespace gpu
} // namespace ibl
