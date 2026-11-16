#include "gfx/SceneRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "gfx/SrvHeap.h"

using util::ThrowIfFailed;
using namespace DirectX;
using namespace gpu;

namespace
{
	struct SceneCB
	{
		XMFLOAT4X4 viewProj;     // ジッタ込み (ラスタライズ用)
		XMFLOAT4X4 world;
		XMFLOAT4X4 invViewProj;  // ジッタ込みの逆行列 (スカイの視線方向)
		XMFLOAT4X4 curViewProj;  // ジッタ無し (速度の計算用)
		XMFLOAT4X4 prevViewProj; // 前フレーム (ジッタ無し)
		XMFLOAT4   cameraPos;
		XMFLOAT4   albedo;     // rgb, a = reflectance
		XMFLOAT4   material;   // metallic, roughness, diffuse, specular
		XMFLOAT4   env;        // exposure, rotation, maxMip, skyLod
		XMFLOAT4   flags;      // useDiffuse, useSpecular, multiScatter, tonemap
		XMFLOAT4   rect;       // シーン矩形の幅, 高さ (px) ※速度をピクセル単位にするため
	};
	static_assert( sizeof( SceneCB ) % 16 == 0, "cbuffer layout" );
	constexpr UINT kCbSize = ( sizeof( SceneCB ) + 255 ) & ~255u;
	constexpr UINT kSceneSrvCount = SrvHeap::kSceneSlots; // ルートシグネチャのテーブル幅
} // namespace

void SceneRenderer::Init( ID3D12Device* device )
{
	CreateRootSignature( device );
	CreatePipelines( device );
	for( UINT i = 0; i < kFrameCount; ++i )
	{
		m_cb[i] = CreateUploadBuffer( device, kCbSize );
		ThrowIfFailed( m_cb[i]->Map( 0, nullptr, reinterpret_cast<void**>( &m_cbMapped[i] ) ), "Map(CB)" );
	}
}

void SceneRenderer::Release()
{
	for( UINT i = 0; i < kFrameCount; ++i )
		if( m_cb[i] ) { m_cb[i]->Unmap( 0, nullptr ); m_cb[i].Reset(); m_cbMapped[i] = nullptr; }
}

void SceneRenderer::CreateRootSignature( ID3D12Device* device )
{
	// ルートシグネチャ: [0] CBV(b0)  [1] SRV テーブル(t0=Prefiltered, t1=LUT, t2=Irradiance)  + 静的サンプラ s0
	D3D12_DESCRIPTOR_RANGE range{};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = kSceneSrvCount;
	range.BaseShaderRegister = 0;
	range.RegisterSpace = 0;
	range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER params[2]{};
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].DescriptorTable.NumDescriptorRanges = 1;
	params[1].DescriptorTable.pDescriptorRanges = &range;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC samp{};
	samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samp.MaxAnisotropy = 1;
	samp.MinLOD = 0.0f;
	samp.MaxLOD = D3D12_FLOAT32_MAX;
	samp.ShaderRegister = 0;
	samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rs{};
	rs.NumParameters = 2;
	rs.pParameters = params;
	rs.NumStaticSamplers = 1;
	rs.pStaticSamplers = &samp;
	rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> blob, err;
	const HRESULT hr = D3D12SerializeRootSignature( &rs, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err );
	if( FAILED( hr ) ) throw std::runtime_error( std::string( "D3D12SerializeRootSignature: " ) + ( err ? static_cast<const char*>( err->GetBufferPointer() ) : "" ) );
	ThrowIfFailed( device->CreateRootSignature( 0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS( &m_rootSig ) ), "CreateRootSignature" );
}

void SceneRenderer::CreatePipelines( ID3D12Device* device )
{
	// ビルド時にコンパイル済みのシェーダー (assets/shaders/pbr_<エントリ名>.cso)
	const auto vsObject = util::LoadShaderBytecode( "pbr_VSMain.cso" );
	const auto psObject = util::LoadShaderBytecode( "pbr_PSMain.cso" );
	const auto vsSky = util::LoadShaderBytecode( "pbr_SkyVS.cso" );
	const auto psSky = util::LoadShaderBytecode( "pbr_SkyPS.cso" );

	const D3D12_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	auto makePso = [&]( const std::vector<std::uint8_t>& vs, const std::vector<std::uint8_t>& ps, bool useLayout, bool depth ) {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
		d.pRootSignature = m_rootSig.Get();
		d.VS = { vs.data(), vs.size() };
		d.PS = { ps.data(), ps.size() };

		auto& rt = d.BlendState.RenderTarget[0];
		rt.BlendEnable = FALSE;
		rt.LogicOpEnable = FALSE;
		rt.SrcBlend = D3D12_BLEND_ONE;       rt.DestBlend = D3D12_BLEND_ZERO;       rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;  rt.DestBlendAlpha = D3D12_BLEND_ZERO;  rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rt.LogicOp = D3D12_LOGIC_OP_NOOP;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		d.SampleMask = UINT_MAX;

		d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 閉じたモデル + 深度テストなので両面描画で問題なし
		d.RasterizerState.DepthClipEnable = TRUE;

		d.DepthStencilState.DepthEnable = depth ? TRUE : FALSE;
		d.DepthStencilState.DepthWriteMask = depth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		d.DepthStencilState.StencilEnable = FALSE;
		d.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		d.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		const D3D12_DEPTH_STENCILOP_DESC stencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
		d.DepthStencilState.FrontFace = stencilOp;
		d.DepthStencilState.BackFace = stencilOp;

		if( useLayout ) d.InputLayout = { layout, static_cast<UINT>( std::size( layout ) ) };
		d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		d.NumRenderTargets = 2; // [0] = カラー, [1] = 速度 (SceneTargets)
		d.RTVFormats[0] = SceneTargets::kColorFormat;
		d.RTVFormats[1] = SceneTargets::kVelocityFormat;
		d.BlendState.IndependentBlendEnable = FALSE;
		d.DSVFormat = kDepthFormat;
		d.SampleDesc.Count = 1;

		ComPtr<ID3D12PipelineState> pso;
		ThrowIfFailed( device->CreateGraphicsPipelineState( &d, IID_PPV_ARGS( &pso ) ), "CreateGraphicsPipelineState" );
		return pso;
	};
	m_psoObject = makePso( vsObject, psObject, true, true );
	m_psoSky = makePso( vsSky, psSky, false, false );
}

void SceneRenderer::Draw( const Frame& f, const Resources& res, const SceneParams& p, const GpuMesh& obj )
{
	ID3D12GraphicsCommandList* cmd = f.cmd;

	// 描画先矩形 (3D View パネルの内側)。ビューポートとシザーを合わせる。背景 (スカイ) は全画面三角形なので、この矩形にだけ描かれる。
	const SceneRect& r = f.rect;
	const D3D12_VIEWPORT sceneVp = { r.x0, r.y0, r.Width(), r.Height(), 0.0f, 1.0f };
	const D3D12_RECT sceneSc = { static_cast<LONG>( std::floor( r.x0 ) ), static_cast<LONG>( std::floor( r.y0 ) ),
								 static_cast<LONG>( std::ceil( r.x1 ) ), static_cast<LONG>( std::ceil( r.y1 ) ) };
	cmd->RSSetViewports( 1, &sceneVp );
	cmd->RSSetScissorRects( 1, &sceneSc );

	const XMMATRIX view = XMLoadFloat4x4( &p.view );
	const XMMATRIX proj = XMLoadFloat4x4( &p.proj );
	const XMMATRIX curVp = view * proj; // ジッタ無し (速度の計算用)

	// TAA のジッタ: 投影行列の (3,1) / (3,2) 成分を足すと、クリップ座標が w 倍されて NDC が一定量ずれる (サブピクセルのシフト)。
	// 行ベクトル規約 (clip = v * M) なので M[2][0] が x、M[2][1] が y。NDC の y は上向きなのでピクセルの下方向とは符号が逆。
	XMFLOAT4X4 jp = p.proj;
	jp.m[2][0] += 2.0f * f.jitterX / r.Width();
	jp.m[2][1] -= 2.0f * f.jitterY / r.Height();
	const XMMATRIX vp = view * XMLoadFloat4x4( &jp );

	SceneCB cb{};
	// HLSL は列優先で読むため、DirectXMath の行列は転置して渡す (HLSL 側は mul(v, M) で使う)
	XMStoreFloat4x4( &cb.viewProj, XMMatrixTranspose( vp ) );
	XMStoreFloat4x4( &cb.world, XMMatrixTranspose( XMMatrixIdentity() ) );
	XMStoreFloat4x4( &cb.invViewProj, XMMatrixTranspose( XMMatrixInverse( nullptr, vp ) ) );
	XMStoreFloat4x4( &cb.curViewProj, XMMatrixTranspose( curVp ) );
	const XMMATRIX prevVp = m_hasPrev ? XMLoadFloat4x4( &m_prevViewProj ) : curVp; // 初回は動き無し扱い
	XMStoreFloat4x4( &cb.prevViewProj, XMMatrixTranspose( prevVp ) );
	cb.cameraPos = { p.cameraPos.x, p.cameraPos.y, p.cameraPos.z, 1.0f };
	cb.albedo = { p.albedo.x, p.albedo.y, p.albedo.z, p.reflectance };
	cb.material = { p.metallic, p.roughness, p.diffuseIntensity, p.specularIntensity };
	const float maxMip = static_cast<float>( res.iblMipCount > 0 ? res.iblMipCount - 1 : 0 );
	cb.env = { std::exp2( p.exposureEV ), p.envRotation, maxMip, p.skyBlur * maxMip };
	cb.flags = { p.useDiffuse ? 1.0f : 0.0f, p.useSpecular ? 1.0f : 0.0f, p.multiScatter ? 1.0f : 0.0f, static_cast<float>( p.tonemap ) };
	cb.rect = { r.Width(), r.Height(), 0.0f, 0.0f };
	std::memcpy( m_cbMapped[f.index], &cb, sizeof( cb ) );

	ID3D12DescriptorHeap* heaps[] = { res.srvHeap };
	cmd->SetDescriptorHeaps( 1, heaps );
	cmd->SetGraphicsRootSignature( m_rootSig.Get() );
	cmd->SetGraphicsRootConstantBufferView( 0, m_cb[f.index]->GetGPUVirtualAddress() );
	cmd->SetGraphicsRootDescriptorTable( 1, res.iblTable );

	if( p.showSky )
	{
		cmd->SetPipelineState( m_psoSky.Get() );
		cmd->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		cmd->DrawInstanced( 3, 1, 0, 0 );
	}

	cmd->SetPipelineState( m_psoObject.Get() );
	cmd->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	cmd->IASetVertexBuffers( 0, 1, &obj.vbv );
	cmd->IASetIndexBuffer( &obj.ibv );
	cmd->DrawIndexedInstanced( obj.indexCount, 1, 0, 0, 0 );

	XMStoreFloat4x4( &m_prevViewProj, curVp );
	m_hasPrev = true;
}
