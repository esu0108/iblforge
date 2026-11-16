#include "gfx/SceneTargets.h"

#include <algorithm>
#include <cstring>

#include "gfx/RenderTargets.h"

using util::ThrowIfFailed;
using namespace gpu;

void SceneTargets::Init( ID3D12Device* device, SrvHeap& srv )
{
	m_device = device;
	m_srv = &srv;
	D3D12_DESCRIPTOR_HEAP_DESC d{};
	d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	d.NumDescriptors = 4;
	ThrowIfFailed( device->CreateDescriptorHeap( &d, IID_PPV_ARGS( &m_rtvHeap ) ), "Scene RTV heap" );
	m_rtvInc = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
}

void SceneTargets::Release()
{
	m_color.Reset();
	m_velocity.Reset();
	m_history[0].Reset();
	m_history[1].Reset();
	m_rtvHeap.Reset();
}

D3D12_CPU_DESCRIPTOR_HANDLE SceneTargets::Rtv( UINT i ) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	h.ptr += static_cast<SIZE_T>( i ) * m_rtvInc;
	return h;
}

ComPtr<ID3D12Resource> SceneTargets::CreateTarget( DXGI_FORMAT format, UINT width, UINT height, const float clear[4] ) const
{
	D3D12_RESOURCE_DESC d{};
	d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d.Width = width;
	d.Height = height;
	d.DepthOrArraySize = 1;
	d.MipLevels = 1;
	d.Format = format;
	d.SampleDesc.Count = 1;
	d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE cv{};
	cv.Format = format;
	std::memcpy( cv.Color, clear, sizeof( cv.Color ) ); // クリア色と一致していると高速クリアになる
	const auto heap = HeapProps( D3D12_HEAP_TYPE_DEFAULT );
	ComPtr<ID3D12Resource> r;
	ThrowIfFailed( m_device->CreateCommittedResource( &heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cv, IID_PPV_ARGS( &r ) ), "Create scene target" );
	return r;
}

void SceneTargets::Resize( UINT width, UINT height )
{
	width = std::max( width, 1u );
	height = std::max( height, 1u );
	m_width = width;
	m_height = height;
	static const float kZero[4] = { 0, 0, 0, 0 };
	m_color = CreateTarget( kColorFormat, width, height, RenderTargets::kClearColor );
	m_velocity = CreateTarget( kVelocityFormat, width, height, kZero );
	m_history[0] = CreateTarget( kColorFormat, width, height, kZero );
	m_history[1] = CreateTarget( kColorFormat, width, height, kZero );

	m_device->CreateRenderTargetView( m_color.Get(), nullptr, Rtv( 0 ) );
	m_device->CreateRenderTargetView( m_velocity.Get(), nullptr, Rtv( 1 ) );
	m_device->CreateRenderTargetView( m_history[0].Get(), nullptr, Rtv( 2 ) );
	m_device->CreateRenderTargetView( m_history[1].Get(), nullptr, Rtv( 3 ) );

	// SRV テーブル A = (シーン, 速度, 履歴 0)、B = (シーン, 速度, 履歴 1)
	auto srv = [&]( UINT slot, ID3D12Resource* r, DXGI_FORMAT f ) {
		D3D12_SHADER_RESOURCE_VIEW_DESC v{};
		v.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		v.Format = f;
		v.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		v.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView( r, &v, m_srv->Cpu( slot ) );
	};
	for( UINT t = 0; t < 2; ++t )
	{
		const UINT base = t == 0 ? SrvHeap::kSlotTaaTableA : SrvHeap::kSlotTaaTableB;
		srv( base + 0, m_color.Get(), kColorFormat );
		srv( base + 1, m_velocity.Get(), kVelocityFormat );
		srv( base + 2, m_history[t].Get(), kColorFormat );
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE SceneTargets::InputTable( UINT historyIndex ) const
{
	return m_srv->Gpu( historyIndex == 0 ? SrvHeap::kSlotTaaTableA : SrvHeap::kSlotTaaTableB );
}

void SceneTargets::BeginScenePass( ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE dsv )
{
	const D3D12_RESOURCE_BARRIER toRt[2] = {
		Transition( m_color.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET ),
		Transition( m_velocity.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET ),
	};
	cmd->ResourceBarrier( 2, toRt );
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2] = { Rtv( 0 ), Rtv( 1 ) };
	cmd->OMSetRenderTargets( 2, rtvs, FALSE, &dsv );
	static const float kZero[4] = { 0, 0, 0, 0 };
	cmd->ClearRenderTargetView( rtvs[0], RenderTargets::kClearColor, 0, nullptr );
	cmd->ClearRenderTargetView( rtvs[1], kZero, 0, nullptr );
	cmd->ClearDepthStencilView( dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
}

void SceneTargets::EndScenePass( ID3D12GraphicsCommandList* cmd )
{
	const D3D12_RESOURCE_BARRIER toSrv[2] = {
		Transition( m_color.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ),
		Transition( m_velocity.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ),
	};
	cmd->ResourceBarrier( 2, toSrv );
}
