// main.cpp : エントリポイント。コマンドライン解析 → (--bake-only なら HeadlessBake) / ウィンドウ + App + イベントループ
//            オプションは app/CommandLine.h を参照。
#include "Common.h"

#include <shellapi.h>

#include "app/App.h"
#include "app/CommandLine.h"
#include "app/EventLoop.h"
#include "app/HeadlessBake.h"
#include "app/MainWindow.h"

int WINAPI wWinMain( HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow )
{
	int argc = 0;
	wchar_t** argv = CommandLineToArgvW( GetCommandLineW(), &argc );
	const CmdLine cmd = ParseCmdLine( argc, argv );
	LocalFree( argv );

	if( cmd.bakeOnly ) return RunHeadless( cmd );

	try
	{
		MainWindow::EnableDpiAwareness();

		MainWindow window;
		window.Create( hInst, 1600, 900, L"iblforge" );

		App app;
		window.SetHandler( &app );
		app.Initialize( window, cmd );

		window.Show( nCmdShow );
		app.OpenInitialHdr( cmd.input ); // 引数で HDRI が渡されたら、読み込み後に自動ベイク

		RunEventLoop( app );

		app.ThrowIfFatal();
		app.Shutdown();
		window.SetHandler( nullptr );
		// app のデストラクタで BakeWorkflow (スレッド join) → Renderer (ImGui / D3D12 解放) の順に破棄される
	}
	catch( const std::exception& e )
	{
		MessageBoxW( nullptr, util::Utf8ToWide( e.what() ).c_str(), L"iblforge - Error", MB_OK | MB_ICONERROR );
		return 1;
	}
	return 0;
}
