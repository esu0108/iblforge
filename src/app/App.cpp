#include "app/App.h"

#include <imgui.h>

#include <algorithm>

#include "app/RenderSync.h"
#include "app/ViewInput.h"
#include "ui/UiContext.h"

void App::Initialize( MainWindow& window, const CmdLine& cmd )
{
	m_hwnd = window.Handle();
	m_dpiScale = window.DpiScale();
	m_model.settings = cmd.settings;
	m_model.results.saveDir = cmd.output; // 引数の outputDir は、保存ダイアログの初期フォルダとして使う

	UINT w = 0, h = 0;
	window.ClientSize( w, h );
	m_renderer.Init( m_hwnd, w, h );
	m_renderer.InitImGui( m_hwnd, m_dpiScale );

	m_ready = true;
}

void App::OpenInitialHdr( const std::wstring& path )
{
	if( !path.empty() ) m_workflow.OpenHdr( path, true );
}

bool App::WantsTextInput() const
{
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput; // カーソル点滅のため
}

void App::OnSize( UINT w, UINT h, bool minimized )
{
	if( minimized ) { m_minimized = true; return; }
	m_minimized = false;
	m_pendingW = w;
	m_pendingH = h;
	// 枠のドラッグ中はメインループに戻らないので、ここで描画する (通常のリサイズ時もこの 1 回で済む)
	try { RenderFrame(); }
	catch( const std::exception& e ) { m_fatalError = e.what(); PostQuitMessage( 1 ); } // 例外を DefWindowProc 経由で外へ出さない
}

void App::Update()
{
	m_fileRequests.Process( m_hwnd, m_model, m_workflow );
	m_workflow.Poll();
	SyncRenderer( m_model, m_renderer );
}

void App::RenderFrame()
{
	if( !m_ready || m_minimized || m_inFrame ) return; // 初期化前 / 最小化中 / 再入 (描画中に WM_SIZE が来た場合) は何もしない
	m_inFrame = true;
	struct Guard { bool& f; ~Guard() { f = false; } } guard{ m_inFrame };

	if( m_pendingW && m_pendingH )
	{
		m_renderer.Resize( m_pendingW, m_pendingH );
		m_pendingW = m_pendingH = 0;
	}
	Update();

	m_renderer.BeginFrame();
	ui::UiContext ctx{ m_model, m_renderer, m_workflow, m_camera, m_hwnd, m_dpiScale };
	ui::BuildMainUi( ctx );
	ProcessCameraInput( m_camera, m_model.viewMouse );
	ProcessShortcuts( m_model, m_workflow );

	SceneParams& scene = m_model.scene;
	// アスペクト比は 3D View パネルの矩形から (ウィンドウ全体ではない)
	const float aspect = ( scene.viewW > 0.0f && scene.viewH > 0.0f )
							 ? scene.viewW / scene.viewH
							 : static_cast<float>( m_renderer.Width() ) / static_cast<float>( std::max( 1u, m_renderer.Height() ) );
	m_camera.Apply( scene, aspect );
	scene.envRotation = DirectX::XMConvertToRadians( m_model.view.envRotationDeg );

	m_renderer.DrawScene( scene );
	m_renderer.EndFrame( m_model.view.vsync );
	m_scheduler.SetTaaActive( m_model.view.taaEnabled ); // TAA は履歴が収束するまで、入力後も多めに描き続ける
	m_scheduler.NoteFrameRendered( ImGui::IsAnyMouseDown() );
}
