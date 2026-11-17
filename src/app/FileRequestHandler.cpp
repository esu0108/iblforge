#include "app/FileRequestHandler.h"

#include <imgui.h>

#include "app/FileDialogs.h"
#include "bake/BakedFileNames.h"
#include "i18n/I18n.h"

namespace
{
	std::wstring Wide( i18n::Str id ) { return util::Utf8ToWide( i18n::Tr( id ) ); }

	// ダイアログのフィルター ("説明\0パターン\0...\0\0" 形式のダブルヌル終端)。std::wstring は内部に \0 を持てる
	std::wstring Filter( i18n::Str desc, const wchar_t* pattern )
	{
		std::wstring f = Wide( desc );
		f.push_back( L'\0' );
		f += pattern;
		f.push_back( L'\0' );
		f += Wide( i18n::Str::DlgAllFiles );
		f.push_back( L'\0' );
		f += L"*.*";
		f.push_back( L'\0' );
		return f; // c_str() の後ろにもう 1 つ終端の \0 が付くのでダブルヌルになる
	}

	// 3 つの DDS のうちどれか 1 つから残りを割り出してインポートする
	void ImportFromFile( const std::filesystem::path& file, AppModel& model, BakeWorkflow& workflow )
	{
		ibl::BakeOutputs o;
		const ibl::DeriveResult r = ibl::DeriveBakedFiles( file, o );
		switch( r.status )
		{
		case ibl::DeriveResult::Status::Ok:
			workflow.ImportFiles( o );
			break;
		case ibl::DeriveResult::Status::MissingFile:
			model.status = i18n::Fmt( i18n::Str::ImportNotFound, util::PathToUtf8( r.missing ).c_str() );
			break;
		case ibl::DeriveResult::Status::NotBakedName:
			model.status = i18n::Tr( i18n::Str::ImportBadName );
			break;
		}
	}
} // namespace

// モーダルダイアログ中はマウス / キーの離した通知が ImGui に届かないので、閉じた後に入力状態を解放しておく
void FileRequestHandler::ReleaseInputAfterDialog()
{
	ImGuiIO& io = ImGui::GetIO();
	io.ClearInputKeys();
	io.ClearInputMouse();
}

void FileRequestHandler::Process( HWND owner, AppModel& model, BakeWorkflow& workflow )
{
	UserRequests& req = model.requests;

	if( req.open )
	{
		req.open = false;
		const auto f = PickOpenFile( owner, Wide( i18n::Str::DlgOpenHdri ).c_str(), Filter( i18n::Str::DlgHdrFilter, L"*.hdr;*.exr" ).c_str() );
		ReleaseInputAfterDialog();
		if( f ) workflow.OpenHdr( *f );
	}
	if( req.import )
	{
		req.import = false;
		const auto f = PickOpenFile( owner, Wide( i18n::Str::DlgImportTitle ).c_str(), Filter( i18n::Str::DlgDdsFilter, L"*.dds" ).c_str() );
		ReleaseInputAfterDialog();
		if( f ) ImportFromFile( *f, model, workflow );
	}
	if( req.save )
	{
		req.save = false;
		if( model.results.hasBaked )
		{
			const auto d = PickFolder( owner, model.results.saveDir, Wide( i18n::Str::DlgSaveTitle ).c_str() );
			ReleaseInputAfterDialog();
			if( d ) workflow.SaveTo( *d );
		}
	}

	// ドロップされたファイル: .hdr / .exr は開く
	if( !req.droppedFile.empty() )
	{
		const std::filesystem::path dropped = req.droppedFile;
		req.droppedFile.clear();
		if( _wcsicmp( dropped.extension().c_str(), L".hdr" ) == 0 || _wcsicmp( dropped.extension().c_str(), L".exr" ) == 0 ) workflow.OpenHdr( dropped );
		else model.status = i18n::Tr( i18n::Str::OnlyHdr );
	}
}
