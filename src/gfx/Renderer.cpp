#include "gfx/Renderer.h"

#include <imgui.h>

#include <algorithm>

static_assert( RenderTargets::kFrameCount == FrameCommands::kFrameCount, "frame count" );

void Renderer::Init( HWND hwnd, UINT width, UINT height )
{
	m_gpu.Init();
	m_srv.Init( m_gpu.Device() );
	m_targets.Init( m_gpu, hwnd, std::max( width, 1u ), std::max( height, 1u ) );
	m_sceneTargets.Init( m_gpu.Device(), m_srv );
	m_sceneTargets.Resize( m_targets.Width(), m_targets.Height() );
	m_frames.Init( m_gpu.Device() );
	m_scene.Init( m_gpu.Device() );
	m_taa.Init( m_gpu.Device() );
	m_meshes.Init( m_gpu.Device() );
	m_ibl.Init( m_gpu, m_srv );
	m_preview.Init( m_gpu, m_srv );
}

void Renderer::Resize( UINT width, UINT height )
{
	m_targets.Resize( width, height ); // GPU の完了を待ってからバックバッファを作り直す (サイズが変わらなければ何もしない)
	if( m_targets.Width() != m_sceneTargets.Width() || m_targets.Height() != m_sceneTargets.Height() )
	{
		m_sceneTargets.Resize( m_targets.Width(), m_targets.Height() ); // WaitIdle 済み
		m_taa.InvalidateHistory();
	}
}

void Renderer::SetTaaEnabled( bool enabled )
{
	if( enabled == m_taaEnabled ) return;
	m_taaEnabled = enabled;
	m_taa.InvalidateHistory(); // 有効化直後は履歴が無い (無効中は履歴を作らない)
}

void Renderer::InitImGui( HWND hwnd, float dpiScale )
{
	m_imgui.Init( hwnd, dpiScale, m_gpu, m_srv, RenderTargets::kFrameCount );
}

void Renderer::Shutdown()
{
	if( !m_gpu.Valid() ) return;
	m_gpu.WaitIdle();
	m_imgui.Shutdown();
	m_preview.Release();
	m_ibl.Release();
	m_meshes.Release();
	m_taa.Release();
	m_scene.Release();
	m_frames.Release();
	m_sceneTargets.Release();
	m_targets.Release();
	m_gpu.Shutdown();
}

void Renderer::BeginFrame()
{
	m_frameIndex = m_targets.CurrentIndex();
	m_cmd = m_frames.Begin( m_gpu, m_frameIndex );
	m_backBufferBegun = false;
	m_imgui.NewFrame();
}

void Renderer::DrawScene( const SceneParams& p )
{
	const SceneRect rect = ComputeSceneRect( p, m_targets.Width(), m_targets.Height() );
	if( !m_ibl.Ready() || !rect.Valid() )
	{
		m_taa.InvalidateHistory(); // このフレームは履歴を作れない
		return;
	}

	// 1) シーン (ジッタ付き) をオフスクリーンへ: カラー + 速度
	SceneRenderer::Frame frame;
	frame.cmd = m_cmd;
	frame.index = m_frameIndex;
	frame.rect = rect;
	if( m_taaEnabled ) m_taa.NextJitter( frame.jitterX, frame.jitterY );

	SceneRenderer::Resources res;
	res.srvHeap = m_srv.Heap();
	res.iblTable = m_ibl.TableGpu();
	res.iblMipCount = m_ibl.MipCount();

	m_sceneTargets.BeginScenePass( m_cmd, m_targets.DepthDsv() );
	m_scene.Draw( frame, res, p, m_meshes.Get( p.object ) );
	m_sceneTargets.EndScenePass( m_cmd );

	// 2) 解決: シーン + 履歴 → バックバッファ (rect の中) と次フレームの履歴
	m_targets.BeginBackBufferPass( m_cmd, m_frameIndex );
	m_backBufferBegun = true;
	TaaResolver::Pass pass;
	pass.cmd = m_cmd;
	pass.frameIndex = m_frameIndex;
	pass.srvHeap = m_srv.Heap();
	pass.backBufferRtv = m_targets.BackBufferRtv( m_frameIndex );
	pass.width = m_targets.Width();
	pass.height = m_targets.Height();
	pass.rect = rect;
	pass.enabled = m_taaEnabled;
	m_taa.Resolve( pass, m_sceneTargets );
	m_targets.BindForUi( m_cmd, m_frameIndex );
}

void Renderer::EndFrame( bool vsync )
{
	if( !m_backBufferBegun ) m_targets.BeginBackBufferPass( m_cmd, m_frameIndex ); // シーンを描かなかったフレーム (IBL 未読み込みなど)
	m_imgui.Render( m_cmd );
	m_targets.EndPass( m_cmd, m_frameIndex );
	m_frames.Execute( m_gpu );
	m_targets.Present( vsync );
	m_frames.MarkSubmitted( m_gpu );
	m_cmd = nullptr;
}
