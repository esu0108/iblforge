#include "gfx/GpuContext.h"

using util::ThrowIfFailed;
using gpu::ComPtr;

void GpuContext::Init()
{
	UINT factoryFlags = 0;
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> dbg;
		if( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &dbg ) ) ) ) { dbg->EnableDebugLayer(); factoryFlags |= DXGI_CREATE_FACTORY_DEBUG; }
	}
#endif
	ThrowIfFailed( CreateDXGIFactory2( factoryFlags, IID_PPV_ARGS( &m_factory ) ), "CreateDXGIFactory2" );

	// 高性能 GPU を優先して選ぶ
	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<IDXGIFactory6> factory6;
	if( SUCCEEDED( m_factory.As( &factory6 ) ) )
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
	ThrowIfFailed( D3D12CreateDevice( adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( &m_device ) ), "D3D12CreateDevice" );

	D3D12_COMMAND_QUEUE_DESC qd{};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed( m_device->CreateCommandQueue( &qd, IID_PPV_ARGS( &m_queue ) ), "CreateCommandQueue" );

	ThrowIfFailed( m_device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &m_fence ) ), "CreateFence" );
	m_event = CreateEventW( nullptr, FALSE, FALSE, nullptr );
}

void GpuContext::Shutdown()
{
	if( !m_device ) return;
	WaitIdle();
	if( m_event ) { CloseHandle( m_event ); m_event = nullptr; }
	m_fence.Reset();
	m_queue.Reset();
	m_device.Reset();
	m_factory.Reset();
}

UINT64 GpuContext::Signal()
{
	const UINT64 v = ++m_counter;
	m_queue->Signal( m_fence.Get(), v );
	return v;
}

void GpuContext::WaitFor( UINT64 value )
{
	if( !m_fence || m_fence->GetCompletedValue() >= value ) return;
	m_fence->SetEventOnCompletion( value, m_event );
	WaitForSingleObject( m_event, INFINITE );
}

void GpuContext::WaitIdle()
{
	if( !m_queue || !m_fence ) return;
	WaitFor( Signal() );
}
