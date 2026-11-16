#include "gfx/RenderTargets.h"

#include <algorithm>
#include <cstring>

using util::ThrowIfFailed;
using namespace gpu;

void RenderTargets::Init( GpuContext& gpuCtx, HWND hwnd, UINT width, UINT height )
{
	m_gpu = &gpuCtx;
	ID3D12Device* dev = m_gpu->Device();
	m_width = std::max( width, 1u );
	m_height = std::max( height, 1u );

	DXGI_SWAP_CHAIN_DESC1 sd{};
	sd.Width = m_width;
	sd.Height = m_height;
	sd.Format = kBackBufferFormat;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = kFrameCount;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	ComPtr<IDXGISwapChain1> sc1;
	ThrowIfFailed( m_gpu->Factory()->CreateSwapChainForHwnd( m_gpu->Queue(), hwnd, &sd, nullptr, nullptr, &sc1 ), "CreateSwapChainForHwnd" );
	ThrowIfFailed( sc1.As( &m_swapChain ), "IDXGISwapChain3" );
	m_gpu->Factory()->MakeWindowAssociation( hwnd, DXGI_MWA_NO_ALT_ENTER );

	D3D12_DESCRIPTOR_HEAP_DESC d{};
	d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; d.NumDescriptors = kFrameCount;
	ThrowIfFailed( dev->CreateDescriptorHeap( &d, IID_PPV_ARGS( &m_rtvHeap ) ), "RTV heap" );
	d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; d.NumDescriptors = 1;
	ThrowIfFailed( dev->CreateDescriptorHeap( &d, IID_PPV_ARGS( &m_dsvHeap ) ), "DSV heap" );
	m_rtvInc = dev->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

	CreateBackBufferResources();
}

void RenderTargets::Release()
{
	ReleaseBackBufferResources();
	m_swapChain.Reset();
	m_rtvHeap.Reset();
	m_dsvHeap.Reset();
}

// ===================================================================== バックバッファ
void RenderTargets::CreateBackBufferResources()
{
	ID3D12Device* dev = m_gpu->Device();
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for( UINT i = 0; i < kFrameCount; ++i )
	{
		ThrowIfFailed( m_swapChain->GetBuffer( i, IID_PPV_ARGS( &m_backBuffers[i] ) ), "GetBuffer" );
		dev->CreateRenderTargetView( m_backBuffers[i].Get(), nullptr, h );
		h.ptr += m_rtvInc;
	}

	D3D12_RESOURCE_DESC dd{};
	dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dd.Width = m_width;
	dd.Height = m_height;
	dd.DepthOrArraySize = 1;
	dd.MipLevels = 1;
	dd.Format = kDepthFormat;
	dd.SampleDesc.Count = 1;
	dd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	D3D12_CLEAR_VALUE cv{};
	cv.Format = kDepthFormat;
	cv.DepthStencil.Depth = 1.0f;
	const auto heap = HeapProps( D3D12_HEAP_TYPE_DEFAULT );
	ThrowIfFailed( dev->CreateCommittedResource( &heap, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS( &m_depth ) ), "Create depth" );
	dev->CreateDepthStencilView( m_depth.Get(), nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart() );

}

void RenderTargets::ReleaseBackBufferResources()
{
	for( auto& b : m_backBuffers ) b.Reset();
	m_depth.Reset();
}

void RenderTargets::Resize( UINT width, UINT height )
{
	width = std::max( width, 1u );
	height = std::max( height, 1u );
	if( !m_gpu || ( width == m_width && height == m_height ) ) return;
	m_gpu->WaitIdle();
	ReleaseBackBufferResources();
	m_width = width;
	m_height = height;
	ThrowIfFailed( m_swapChain->ResizeBuffers( kFrameCount, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0 ), "ResizeBuffers" );
	CreateBackBufferResources();
}

// ===================================================================== パス
D3D12_CPU_DESCRIPTOR_HANDLE RenderTargets::BackBufferRtv( UINT frameIndex ) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	h.ptr += static_cast<SIZE_T>( frameIndex ) * m_rtvInc;
	return h;
}

void RenderTargets::BeginBackBufferPass( ID3D12GraphicsCommandList* cmd, UINT frameIndex )
{
	const auto toRt = Transition( m_backBuffers[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET );
	cmd->ResourceBarrier( 1, &toRt );
	BindForUi( cmd, frameIndex );
	cmd->ClearRenderTargetView( BackBufferRtv( frameIndex ), kClearColor, 0, nullptr );

	const D3D12_VIEWPORT vp = { 0.0f, 0.0f, static_cast<float>( m_width ), static_cast<float>( m_height ), 0.0f, 1.0f };
	const D3D12_RECT sc = { 0, 0, static_cast<LONG>( m_width ), static_cast<LONG>( m_height ) };
	cmd->RSSetViewports( 1, &vp );
	cmd->RSSetScissorRects( 1, &sc );
}

void RenderTargets::BindForUi( ID3D12GraphicsCommandList* cmd, UINT frameIndex )
{
	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = BackBufferRtv( frameIndex );
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = DepthDsv(); // ImGui の PSO の DSV 形式と合わせるために束ねる (深度テストは使わない)
	cmd->OMSetRenderTargets( 1, &rtv, FALSE, &dsv );
}

void RenderTargets::EndPass( ID3D12GraphicsCommandList* cmd, UINT frameIndex )
{
	const auto toPresent = Transition( m_backBuffers[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT );
	cmd->ResourceBarrier( 1, &toPresent );
}

void RenderTargets::Present( bool vsync )
{
	ThrowIfFailed( m_swapChain->Present( vsync ? 1 : 0, 0 ), "Present" );
}
