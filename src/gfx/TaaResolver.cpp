#include "gfx/TaaResolver.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

using util::ThrowIfFailed;
using namespace gpu;

namespace
{
	struct ResolveCB
	{
		float rect[4];    // x0, y0, x1, y1
		float params[4];  // enabled, reset, alphaStatic, alphaMoving
		float screen[4];  // w, h, 1/w, 1/h
	};
	constexpr UINT kCbSize = ( sizeof( ResolveCB ) + 255 ) & ~255u;

	constexpr float kAlphaStatic = 0.1f; // 静止時: 今フレームの重み (履歴 90%) → 約 20 フレームの平均 = 16 サンプルのジッタが十分に混ざる
	constexpr float kAlphaMoving = 0.5f; // 動いている時

	float Halton( UINT index, UINT base )
	{
		float f = 1.0f, r = 0.0f;
		while( index > 0 )
		{
			f /= static_cast<float>( base );
			r += f * static_cast<float>( index % base );
			index /= base;
		}
		return r;
	}
} // namespace

void TaaResolver::Init( ID3D12Device* device )
{
	CreatePipeline( device );
	for( UINT i = 0; i < kFrameCount; ++i )
	{
		m_cb[i] = CreateUploadBuffer( device, kCbSize );
		ThrowIfFailed( m_cb[i]->Map( 0, nullptr, reinterpret_cast<void**>( &m_cbMapped[i] ) ), "Map(TAA CB)" );
	}
}

void TaaResolver::Release()
{
	for( UINT i = 0; i < kFrameCount; ++i )
		if( m_cb[i] ) { m_cb[i]->Unmap( 0, nullptr ); m_cb[i].Reset(); m_cbMapped[i] = nullptr; }
	m_pso.Reset();
	m_rootSig.Reset();
}

void TaaResolver::CreatePipeline( ID3D12Device* device )
{
	// ルートシグネチャ: [0] CBV(b0)  [1] SRV テーブル(t0=シーン, t1=速度, t2=履歴)  + 静的サンプラ s0 (linear / clamp)
	D3D12_DESCRIPTOR_RANGE range{};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = SrvHeap::kTaaTableSize;
	range.BaseShaderRegister = 0;
	range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER params[2]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &range;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC samp{};
	samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samp.MaxAnisotropy = 1;
	samp.MaxLOD = D3D12_FLOAT32_MAX;
	samp.ShaderRegister = 0;
	samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 2;
	rs.pParameters = params;
	rs.NumStaticSamplers = 1;
	rs.pStaticSamplers = &samp;
	ComPtr<ID3DBlob> blob, err;
	const HRESULT hr = D3D12SerializeRootSignature( &rs, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err );
	if( FAILED( hr ) ) throw std::runtime_error( std::string( "D3D12SerializeRootSignature(TAA): " ) + ( err ? static_cast<const char*>( err->GetBufferPointer() ) : "" ) );
	ThrowIfFailed( device->CreateRootSignature( 0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS( &m_rootSig ) ), "CreateRootSignature(TAA)" );

	// ビルド時にコンパイル済みのシェーダー
	const auto vs = util::LoadShaderBytecode( "taa_ResolveVS.cso" );
	const auto ps = util::LoadShaderBytecode( "taa_ResolvePS.cso" );

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
	d.pRootSignature = m_rootSig.Get();
	d.VS = { vs.data(), vs.size() };
	d.PS = { ps.data(), ps.size() };
	for( auto& rt : d.BlendState.RenderTarget )
	{
		rt.SrcBlend = D3D12_BLEND_ONE;       rt.DestBlend = D3D12_BLEND_ZERO;       rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;  rt.DestBlendAlpha = D3D12_BLEND_ZERO;  rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rt.LogicOp = D3D12_LOGIC_OP_NOOP;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	d.SampleMask = UINT_MAX;
	d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	d.RasterizerState.DepthClipEnable = TRUE;
	d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d.NumRenderTargets = 2; // [0] = バックバッファ, [1] = 履歴
	d.RTVFormats[0] = kBackBufferFormat;
	d.RTVFormats[1] = SceneTargets::kColorFormat;
	d.SampleDesc.Count = 1; // 深度は使わない (DSV 無し)
	ThrowIfFailed( device->CreateGraphicsPipelineState( &d, IID_PPV_ARGS( &m_pso ) ), "CreateGraphicsPipelineState(TAA)" );
}

void TaaResolver::NextJitter( float& jitterX, float& jitterY )
{
	const UINT i = m_jitterIndex % 16 + 1; // Halton は index 0 が (0,0) なので 1 から
	++m_jitterIndex;
	jitterX = Halton( i, 2 ) - 0.5f;
	jitterY = Halton( i, 3 ) - 0.5f;
}

void TaaResolver::Resolve( const Pass& pass, SceneTargets& targets )
{
	ID3D12GraphicsCommandList* cmd = pass.cmd;
	const UINT write = 1 - m_read;

	// 矩形 / サイズが変わったら履歴は無効 (別の画面の内容)
	const SceneRect& r = pass.rect;
	if( pass.width != m_lastWidth || pass.height != m_lastHeight ||
		r.x0 != m_lastRect.x0 || r.y0 != m_lastRect.y0 || r.x1 != m_lastRect.x1 || r.y1 != m_lastRect.y1 )
		m_historyValid = false;
	m_lastRect = r;
	m_lastWidth = pass.width;
	m_lastHeight = pass.height;
	const bool reset = !pass.enabled || !m_historyValid;

	ResolveCB cb{};
	cb.rect[0] = std::floor( r.x0 ); cb.rect[1] = std::floor( r.y0 ); cb.rect[2] = std::ceil( r.x1 ); cb.rect[3] = std::ceil( r.y1 );
	cb.params[0] = pass.enabled ? 1.0f : 0.0f;
	cb.params[1] = reset ? 1.0f : 0.0f;
	cb.params[2] = kAlphaStatic;
	cb.params[3] = kAlphaMoving;
	cb.screen[0] = static_cast<float>( pass.width );  cb.screen[1] = static_cast<float>( pass.height );
	cb.screen[2] = 1.0f / cb.screen[0];               cb.screen[3] = 1.0f / cb.screen[1];
	std::memcpy( m_cbMapped[pass.frameIndex], &cb, sizeof( cb ) );

	// 履歴 (書く側) を RenderTarget に。読む側は PIXEL_SHADER_RESOURCE のまま
	const auto toRt = Transition( targets.History( write ), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET );
	cmd->ResourceBarrier( 1, &toRt );
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvs[2] = { pass.backBufferRtv, targets.HistoryRtv( write ) };
	cmd->OMSetRenderTargets( 2, rtvs, FALSE, nullptr );

	const D3D12_VIEWPORT vp = { cb.rect[0], cb.rect[1], cb.rect[2] - cb.rect[0], cb.rect[3] - cb.rect[1], 0.0f, 1.0f };
	const D3D12_RECT sc = { static_cast<LONG>( cb.rect[0] ), static_cast<LONG>( cb.rect[1] ), static_cast<LONG>( cb.rect[2] ), static_cast<LONG>( cb.rect[3] ) };
	cmd->RSSetViewports( 1, &vp );
	cmd->RSSetScissorRects( 1, &sc );

	ID3D12DescriptorHeap* heaps[] = { pass.srvHeap };
	cmd->SetDescriptorHeaps( 1, heaps );
	cmd->SetGraphicsRootSignature( m_rootSig.Get() );
	cmd->SetPipelineState( m_pso.Get() );
	cmd->SetGraphicsRootConstantBufferView( 0, m_cb[pass.frameIndex]->GetGPUVirtualAddress() );
	cmd->SetGraphicsRootDescriptorTable( 1, targets.InputTable( m_read ) );
	cmd->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	cmd->DrawInstanced( 3, 1, 0, 0 );

	const auto toSrv = Transition( targets.History( write ), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
	cmd->ResourceBarrier( 1, &toSrv );

	m_read = write; // 今書いた履歴が、次フレームの「前フレーム」
	m_historyValid = pass.enabled;
}
