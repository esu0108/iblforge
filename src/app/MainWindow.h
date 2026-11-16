// MainWindow.h : Win32 のメインウィンドウ (クラス登録 / 作成 / WndProc)。ウィンドウイベントは IWindowHandler へ渡すだけで、描画やアプリの状態は知らない。
#pragma once
#include "Common.h"

#include <string>

class IWindowHandler
{
public:
	virtual ~IWindowHandler() = default;
	virtual void OnSize( UINT clientWidth, UINT clientHeight, bool minimized ) = 0;
	virtual void OnFileDropped( const std::wstring& path ) = 0;
};

class MainWindow
{
public:
	MainWindow() = default;
	MainWindow( const MainWindow& ) = delete;
	MainWindow& operator=( const MainWindow& ) = delete;

	static void EnableDpiAwareness(); // ウィンドウ作成より前に呼ぶ

	// クライアント領域が clientWidth x clientHeight のウィンドウを作る (表示はしない)。例外を投げる
	void Create( HINSTANCE hInst, int clientWidth, int clientHeight, const wchar_t* title );
	void Show( int nCmdShow );
	void SetHandler( IWindowHandler* handler ) { m_handler = handler; }

	HWND Handle() const { return m_hwnd; }
	float DpiScale() const;
	void ClientSize( UINT& width, UINT& height ) const;

private:
	static LRESULT CALLBACK Proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
	LRESULT HandleMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );

	HWND m_hwnd = nullptr;
	IWindowHandler* m_handler = nullptr;
};
