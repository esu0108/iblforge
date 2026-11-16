// App.h : アプリ本体。Renderer / モデル / ワークフロー / UI を組み合わせて 1 フレームを回す (ウィンドウイベントは IWindowHandler で受ける)
#pragma once
#include "Common.h"

#include <string>

#include "app/AppModel.h"
#include "app/BakeWorkflow.h"
#include "app/CommandLine.h"
#include "app/FileRequestHandler.h"
#include "app/FrameScheduler.h"
#include "app/MainWindow.h"
#include "app/OrbitCamera.h"
#include "gfx/Renderer.h"

class App : public IWindowHandler
{
public:
	App() : m_workflow( m_model, m_renderer ) {}

	// Renderer / ImGui を初期化し、コマンドライン設定を反映する (ウィンドウ作成後、表示前に呼ぶ)。例外を投げる
	void Initialize( MainWindow& window, const CmdLine& cmd );
	// コマンドラインで HDRI が渡されていれば、読み込み後に自動ベイクする (保存は File > Save...)
	void OpenInitialHdr( const std::wstring& path );
	void Shutdown() { m_renderer.WaitForGpu(); }

	// 1 フレーム分の更新 + 描画。メインループと、リサイズ中 (WM_SIZE) の両方から呼ばれる。
	//   ウィンドウ枠のドラッグ中は DefWindowProc が内部ループを回してメインループに戻らないため、
	//   WM_SIZE から直接呼んで、リサイズ中も 3D とパネルが追従して描画されるようにする。
	void RenderFrame();

	// ---- IWindowHandler
	void OnSize( UINT clientWidth, UINT clientHeight, bool minimized ) override;
	void OnFileDropped( const std::wstring& path ) override { m_model.requests.droppedFile = path; }

	// ---- メインループ用の問い合わせ
	FrameScheduler& Scheduler() { return m_scheduler; }
	bool Minimized() const { return m_minimized; }
	bool ContinuousRender() const { return m_model.view.alwaysRender; }
	bool Busy() const { return m_workflow.IsBusy(); }
	bool WantsTextInput() const;
	void ThrowIfFatal() const { if( !m_fatalError.empty() ) throw std::runtime_error( m_fatalError ); }

private:
	void Update(); // フレーム開始前に呼ぶ (ダイアログと GPU リソースの入れ替えがあるため)

	// 宣言順 = 生成順 (破棄は逆順: ワークフロー (ベイクスレッドの join) → Renderer の順になるようにする)
	AppModel m_model;
	Renderer m_renderer;
	BakeWorkflow m_workflow;
	FileRequestHandler m_fileRequests;
	OrbitCamera m_camera;
	FrameScheduler m_scheduler;

	HWND m_hwnd = nullptr;
	float m_dpiScale = 1.0f;
	UINT m_pendingW = 0, m_pendingH = 0;
	bool m_minimized = false;
	bool m_ready = false;       // Renderer / ImGui の初期化が済んだ (それまで RenderFrame は何もしない)
	bool m_inFrame = false;
	std::string m_fatalError;   // WM_SIZE 内の描画で例外が出た場合の内容 (メインループが投げ直す)
};
