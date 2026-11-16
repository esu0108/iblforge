#include "app/FileRequestHandler.h"

#include <imgui.h>

#include "app/FileDialogs.h"
#include "bake/BakedFileNames.h"

namespace
{
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
			model.status = "Import failed: File not found: " + util::PathToUtf8( r.missing );
			break;
		case ibl::DeriveResult::Status::NotBakedName:
			model.status = "Import failed: Select one of <name>_prefiltered.dds / <name>_brdf_lut.dds / <name>_irradiance.dds.";
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
		const auto f = PickOpenFile( owner, L"Open HDRI", L"HDR image (*.hdr)\0*.hdr\0All files (*.*)\0*.*\0" );
		ReleaseInputAfterDialog();
		if( f ) workflow.OpenHdr( *f );
	}
	if( req.import )
	{
		req.import = false;
		const auto f = PickOpenFile( owner, L"Import baked textures (select any one of the 3 files)", L"Baked texture (*.dds)\0*.dds\0All files (*.*)\0*.*\0" );
		ReleaseInputAfterDialog();
		if( f ) ImportFromFile( *f, model, workflow );
	}
	if( req.save )
	{
		req.save = false;
		if( model.results.hasBaked )
		{
			const auto d = PickFolder( owner, model.results.saveDir, L"Select the folder to save the 3 baked textures" );
			ReleaseInputAfterDialog();
			if( d ) workflow.SaveTo( *d );
		}
	}

	// ドロップされたファイル: .hdr は開く
	if( !req.droppedFile.empty() )
	{
		const std::filesystem::path dropped = req.droppedFile;
		req.droppedFile.clear();
		if( _wcsicmp( dropped.extension().c_str(), L".hdr" ) == 0 ) workflow.OpenHdr( dropped );
		else model.status = "Only .hdr files can be opened.";
	}
}
