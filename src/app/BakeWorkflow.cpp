#include "app/BakeWorkflow.h"

#include <cstdio>

#include "bake/ShFile.h"

#include "Common.h"
#include "i18n/I18n.h"

void BakeWorkflow::OpenHdr( const std::filesystem::path& path, bool thenBake )
{
	if( IsLoading() ) { m_model.status.Set( i18n::Str::AnotherLoading ); return; }
	m_bakeAfterLoad = thenBake;
	m_model.status.Set( i18n::Str::LoadingFile, util::PathToUtf8( path.filename() ).c_str() );
	m_loader.Start( path );
}

void BakeWorkflow::StartBake()
{
	if( !m_model.hdr.Loaded() ) { m_model.status.Set( i18n::Str::OpenFirst ); return; }
	ibl::BakeJob job;
	job.hdr      = m_model.hdr.image;
	job.stem     = m_model.hdr.path.stem().wstring();
	job.settings = m_model.settings;
	job.device   = m_renderer.Device(); // GPU ベイクはビューアと同じ D3D12 デバイスを使う (ワークフローは Renderer より先に破棄される)
	if( m_bake.Start( std::move( job ) ) ) m_model.status.Set( i18n::Str::Baking );
}

// 3D ビューが拡散光に使う SH (E/π) を更新する。radiance が nullptr なら「SH 無し」(UI で選べなくなり、描画は Irradiance Cube にフォールバック)
void BakeWorkflow::SetSceneSh( const ibl::SH9* radiance )
{
	SceneParams& sc = m_model.scene;
	sc.shAvailable = radiance != nullptr;
	if( !radiance ) return;
	const ibl::SH9 e = ibl::IrradianceOverPiSH( *radiance );
	for( int i = 0; i < 9; ++i ) sc.sh[i] = { e.c[i].x, e.c[i].y, e.c[i].z };
}

bool BakeWorkflow::ShowBaked( ibl::BakedTextures baked )
{
	std::string err;
	if( !m_renderer.LoadIBL( baked, err ) ) { m_model.status.Set( i18n::Str::UploadFailed, err.c_str() ); return false; }
	SetSceneSh( &baked.sh );
	m_model.results.baked = std::move( baked );
	m_model.results.hasBaked = true;
	m_model.results.bakedSaved = false;
	return true;
}

void BakeWorkflow::ImportFiles( const ibl::BakeOutputs& o )
{
	std::string err;
	if( !m_renderer.LoadIBL( o.prefilteredCube, o.brdfLut, o.irradianceCube, err ) ) { m_model.status.Set( i18n::Str::ImportFailed, err.c_str() ); return; }
	// SH 係数 (<stem>_sh.json) は任意。無ければ 3D ビューでは Irradiance Cube だけを使える
	ibl::SH9 sh;
	std::error_code ec;
	if( std::filesystem::exists( o.sh, ec ) && ibl::ReadShJson( o.sh, sh, err ) ) SetSceneSh( &sh );
	else SetSceneSh( nullptr );
	BakeResults& r = m_model.results;
	r.outputs = o;
	r.hasOutputs = true;
	r.baked = ibl::BakedTextures{};
	r.hasBaked = false;
	r.bakedSaved = false;
	m_model.status.Set( i18n::Str::Imported, util::PathToUtf8( o.prefilteredCube.parent_path() ).c_str() );
}

void BakeWorkflow::SaveTo( const std::filesystem::path& dir )
{
	ibl::BakeOutputs o;
	std::string err;
	if( !ibl::SaveBaked( m_model.results.baked, dir, o, err ) ) { m_model.status.Set( i18n::Str::SaveFailed, err.c_str() ); return; }
	BakeResults& r = m_model.results;
	r.outputs = o;
	r.hasOutputs = true;
	r.bakedSaved = true;
	r.saveDir = dir;
	m_model.status.Set( i18n::Str::SavedTo, util::PathToUtf8( dir ).c_str() );
}

void BakeWorkflow::Poll()
{
	HdrLoadResult loaded;
	if( m_loader.TryTake( loaded ) ) OnLoadFinished( loaded );

	const BakeController::State st = m_bake.GetState();
	if( st == BakeController::State::Finished || st == BakeController::State::Failed || st == BakeController::State::Canceled ) OnBakeFinished( st );
}

void BakeWorkflow::OnLoadFinished( HdrLoadResult& r )
{
	if( !r.ok )
	{
		m_model.status.Set( i18n::Str::OpenFailed, r.err.c_str() );
		m_bakeAfterLoad = false;
		return;
	}
	HdrDocument& d = m_model.hdr;
	d.image = r.image;
	d.path = r.path;
	d.preview = std::move( r.preview );
	d.previewEV = d.preview.autoEV;
	d.previewDirty = true;
	m_model.status.Set( i18n::Str::Opened, util::PathToUtf8( d.path.filename() ).c_str() );
	if( m_bakeAfterLoad ) StartBake();
	m_bakeAfterLoad = false;
}

void BakeWorkflow::OnBakeFinished( BakeController::State state )
{
	ibl::BakedTextures out;
	std::string err;
	double sec = 0.0;
	bool usedGpu = false;
	m_bake.Take( out, err, sec, usedGpu );
	if( state == BakeController::State::Canceled ) { m_model.status.Set( i18n::Str::BakeCanceled ); return; }
	if( state == BakeController::State::Failed ) { m_model.status.Set( i18n::Str::BakeFailed, err.c_str() ); return; }
	if( ShowBaked( std::move( out ) ) )
	{
		m_model.status.Set( i18n::Str::BakeFinished, usedGpu ? "GPU" : "CPU", sec, ibl::CubeFormatName( m_model.settings.cubeFormat ) );
	}
}
