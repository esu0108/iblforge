#include "FileDialogs.h"

#include <commdlg.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace
{
	struct ComScope // IFileDialog 用の COM 初期化 (既に初期化済みでも、モード違いでも続行できる)
	{
		HRESULT hr = CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );
		~ComScope() { if( SUCCEEDED( hr ) ) CoUninitialize(); }
	};
} // namespace

std::optional<std::filesystem::path> PickFolder( HWND owner, const std::filesystem::path& initial, const wchar_t* title )
{
	ComScope com;
	Microsoft::WRL::ComPtr<IFileOpenDialog> dlg; // com より後に宣言 = com より先に解放される
	if( FAILED( CoCreateInstance( CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &dlg ) ) ) ) return std::nullopt;
	DWORD opts = 0;
	dlg->GetOptions( &opts );
	dlg->SetOptions( opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST );
	dlg->SetTitle( title );

	std::error_code ec;
	const std::filesystem::path cur = std::filesystem::absolute( initial, ec );
	if( !ec && !initial.empty() && std::filesystem::is_directory( cur, ec ) )
	{
		Microsoft::WRL::ComPtr<IShellItem> item;
		if( SUCCEEDED( SHCreateItemFromParsingName( cur.c_str(), nullptr, IID_PPV_ARGS( &item ) ) ) ) dlg->SetFolder( item.Get() );
	}
	if( FAILED( dlg->Show( owner ) ) ) return std::nullopt; // キャンセル
	Microsoft::WRL::ComPtr<IShellItem> result;
	if( FAILED( dlg->GetResult( &result ) ) ) return std::nullopt;
	PWSTR path = nullptr;
	if( FAILED( result->GetDisplayName( SIGDN_FILESYSPATH, &path ) ) || !path ) return std::nullopt;
	std::filesystem::path out = path;
	CoTaskMemFree( path );
	return out;
}

std::optional<std::filesystem::path> PickOpenFile( HWND owner, const wchar_t* title, const wchar_t* filter )
{
	wchar_t file[MAX_PATH * 2] = {};
	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof( ofn );
	ofn.hwndOwner = owner;
	ofn.lpstrTitle = title;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = file;
	ofn.nMaxFile = static_cast<DWORD>( std::size( file ) );
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if( !GetOpenFileNameW( &ofn ) ) return std::nullopt;
	return std::filesystem::path( file );
}
