#include "resource.h"
#include "app/MainWindow.h"

#include <dwmapi.h>
#include <shellapi.h>

#include <imgui.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

namespace
{
	// Windows のアプリテーマ (設定 > 個人用設定 > 色 > 既定のアプリモード) がダークか
	bool IsAppDarkMode()
	{
		DWORD value = 1; // 取得できなければライト扱い
		DWORD size = sizeof( value );
		if( RegGetValueW( HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
						  RRF_RT_REG_DWORD, nullptr, &value, &size ) != ERROR_SUCCESS )
			return false;
		return value == 0;
	}

	// タイトルバーをアプリテーマに合わせる (Windows 10 1809 以降。未対応の OS では何も起きない)
	void ApplyTitleBarTheme( HWND hwnd )
	{
		const BOOL dark = IsAppDarkMode() ? TRUE : FALSE;
		constexpr DWORD kUseImmersiveDarkMode = 20;       // DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 20H1 以降 / 11)
		constexpr DWORD kUseImmersiveDarkModeBefore20H1 = 19; // 1809 〜 1909
		if( FAILED( DwmSetWindowAttribute( hwnd, kUseImmersiveDarkMode, &dark, sizeof( dark ) ) ) )
			DwmSetWindowAttribute( hwnd, kUseImmersiveDarkModeBefore20H1, &dark, sizeof( dark ) );
	}
} // namespace

void MainWindow::EnableDpiAwareness()
{
	ImGui_ImplWin32_EnableDpiAwareness();
}

void MainWindow::Create( HINSTANCE hInst, int clientWidth, int clientHeight, const wchar_t* title )
{
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof( wc );
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = &MainWindow::Proc;
	wc.hInstance = hInst;
	wc.hIcon = LoadIconW( hInst, MAKEINTRESOURCEW( IDI_APPICON ) );
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursorW( nullptr, IDC_ARROW );
	wc.lpszClassName = L"IBLForgeWindow";
	RegisterClassExW( &wc );

	RECT rc = { 0, 0, clientWidth, clientHeight };
	AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	m_hwnd = CreateWindowExW( 0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW,
							  CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, this );
	if( !m_hwnd ) throw std::runtime_error( "CreateWindowEx failed" );
	ApplyTitleBarTheme( m_hwnd );
	DragAcceptFiles( m_hwnd, TRUE );
}

void MainWindow::Show( int nCmdShow )
{
	ShowWindow( m_hwnd, nCmdShow );
	UpdateWindow( m_hwnd );
}

float MainWindow::DpiScale() const
{
	return ImGui_ImplWin32_GetDpiScaleForHwnd( m_hwnd );
}

void MainWindow::ClientSize( UINT& width, UINT& height ) const
{
	RECT client{};
	GetClientRect( m_hwnd, &client );
	width = static_cast<UINT>( client.right - client.left );
	height = static_cast<UINT>( client.bottom - client.top );
}

LRESULT CALLBACK MainWindow::Proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	MainWindow* self = nullptr;
	if( msg == WM_NCCREATE )
	{
		self = static_cast<MainWindow*>( reinterpret_cast<CREATESTRUCTW*>( lp )->lpCreateParams );
		SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( self ) );
	}
	else
		self = reinterpret_cast<MainWindow*>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );

	if( ImGui_ImplWin32_WndProcHandler( hwnd, msg, wp, lp ) ) return true;
	if( self ) return self->HandleMessage( hwnd, msg, wp, lp );
	return DefWindowProcW( hwnd, msg, wp, lp );
}

LRESULT MainWindow::HandleMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
	switch( msg )
	{
	case WM_SIZE:
		if( m_handler ) m_handler->OnSize( LOWORD( lp ), HIWORD( lp ), wp == SIZE_MINIMIZED );
		return 0;
	case WM_SETTINGCHANGE: // 実行中にテーマが切り替わったら追従する
		if( lp && lstrcmpW( reinterpret_cast<const wchar_t*>( lp ), L"ImmersiveColorSet" ) == 0 ) ApplyTitleBarTheme( hwnd );
		break;
	case WM_ERASEBKGND:
		return 1; // 背景消去しない (D3D12 が全面を描くのでちらつき防止)
	case WM_DROPFILES:
	{
		HDROP drop = reinterpret_cast<HDROP>( wp );
		const UINT n = DragQueryFileW( drop, 0, nullptr, 0 );
		std::wstring path( n, L'\0' );
		DragQueryFileW( drop, 0, path.data(), n + 1 );
		DragFinish( drop );
		if( m_handler ) m_handler->OnFileDropped( path );
		return 0;
	}
	case WM_SYSCOMMAND:
		if( ( wp & 0xFFF0 ) == SC_KEYMENU ) return 0; // Alt キーでのメニュー起動を抑止
		break;
	case WM_DESTROY:
		PostQuitMessage( 0 );
		return 0;
	}
	return DefWindowProcW( hwnd, msg, wp, lp );
}
