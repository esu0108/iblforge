#include "app/BakeWorkflow.h"

#include <cstdio>

#include "Common.h"

void BakeWorkflow::OpenHdr( const std::filesystem::path& path, bool thenBake )
{
	if( IsLoading() ) { m_model.status = "Another HDRI is still loading."; return; }
	m_bakeAfterLoad = thenBake;
	m_model.status = "Loading " + util::PathToUtf8( path.filename() ) + " ...";
	m_loader.Start( path );
}

void BakeWorkflow::StartBake()
{
	if( !m_model.hdr.Loaded() ) { m_model.status = "Open an HDRI first (File > Open...)."; return; }
	ibl::BakeJob job;
	job.hdr      = m_model.hdr.image;
	job.stem     = m_model.hdr.path.stem().wstring();
	job.settings = m_model.settings;
	job.device   = m_renderer.Device(); // GPU ベイクはビューアと同じ D3D12 デバイスを使う (ワークフローは Renderer より先に破棄される)
	if( m_bake.Start( std::move( job ) ) ) m_model.status = "Baking...";
}

bool BakeWorkflow::ShowBaked( ibl::BakedTextures baked )
{
	std::string err;
	if( !m_renderer.LoadIBL( baked, err ) ) { m_model.status = "Upload failed: " + err; return false; }
	m_model.results.baked = std::move( baked );
	m_model.results.hasBaked = true;
	m_model.results.bakedSaved = false;
	return true;
}

void BakeWorkflow::ImportFiles( const ibl::BakeOutputs& o )
{
	std::string err;
	if( !m_renderer.LoadIBL( o.prefilteredCube, o.brdfLut, o.irradianceCube, err ) ) { m_model.status = "Import failed: " + err; return; }
	BakeResults& r = m_model.results;
	r.outputs = o;
	r.hasOutputs = true;
	r.baked = ibl::BakedTextures{};
	r.hasBaked = false;
	r.bakedSaved = false;
	m_model.status = "Imported baked textures from: " + util::PathToUtf8( o.prefilteredCube.parent_path() );
}

void BakeWorkflow::SaveTo( const std::filesystem::path& dir )
{
	ibl::BakeOutputs o;
	std::string err;
	if( !ibl::SaveBaked( m_model.results.baked, dir, o, err ) ) { m_model.status = "Save failed: " + err; return; }
	BakeResults& r = m_model.results;
	r.outputs = o;
	r.hasOutputs = true;
	r.bakedSaved = true;
	r.saveDir = dir;
	m_model.status = "Saved 3 files to: " + util::PathToUtf8( dir );
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
		m_model.status = "Failed to open HDRI: " + r.err;
		m_bakeAfterLoad = false;
		return;
	}
	HdrDocument& d = m_model.hdr;
	d.image = r.image;
	d.path = r.path;
	d.preview = std::move( r.preview );
	d.previewEV = d.preview.autoEV;
	d.previewDirty = true;
	m_model.status = "Opened " + util::PathToUtf8( d.path.filename() ) + ". Press Bake.";
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
	if( state == BakeController::State::Canceled ) { m_model.status = "Bake canceled."; return; }
	if( state == BakeController::State::Failed ) { m_model.status = "Bake failed: " + err; return; }
	if( ShowBaked( std::move( out ) ) )
	{
		char buf[160];
		std::snprintf( buf, sizeof( buf ), "Bake finished on %s in %.2f s (%s). Not saved yet: use File > Save...", usedGpu ? "GPU" : "CPU", sec, ibl::CubeFormatName( m_model.settings.cubeFormat ) );
		m_model.status = buf;
	}
}
