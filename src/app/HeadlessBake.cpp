#include "HeadlessBake.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>

#include "Common.h"
#include "bake/IBLIO.h"

int RunHeadless( const CmdLine& c )
{
	if( AttachConsole( ATTACH_PARENT_PROCESS ) || AllocConsole() )
	{
		FILE* f = nullptr;
		freopen_s( &f, "CONOUT$", "w", stdout );
		freopen_s( &f, "CONOUT$", "w", stderr );
	}
	if( c.input.empty() ) { std::fprintf( stderr, "usage: iblforge --bake-only <input.hdr|input.exr> [outputDir] [options]\n" ); return 2; }

	// HDRI は先に読み込み、ベイクには読み込み済みの画像を渡す (GUI と同じ流れ)
	auto hdr = std::make_shared<ibl::EquirectImage>();
	{
		std::string loadErr;
		if( !ibl::LoadEquirectHdr( c.input, *hdr, loadErr ) ) { std::fprintf( stderr, "ERROR: %s\n", loadErr.c_str() ); return 1; }
	}
	ibl::BakeJob job;
	job.hdr      = hdr;
	job.stem     = std::filesystem::path( c.input ).stem().wstring();
	job.settings = c.settings;

	std::mutex m;
	const char* lastStage = nullptr;
	int lastPct = -1;
	auto progress = [&]( const char* stage, float p ) {
		std::lock_guard<std::mutex> lock( m );
		const int pct = static_cast<int>( p * 100.0f );
		if( stage != lastStage || pct != lastPct )
		{
			std::printf( "\r%-24s %3d%%   ", stage, pct );
			std::fflush( stdout );
			lastStage = stage;
			lastPct = pct;
		}
	};

	ibl::BakedTextures baked;
	ibl::BakeOutputs out;
	std::string err;
	const auto t0 = std::chrono::steady_clock::now();
	const bool ok = ibl::RunBake( job, progress, baked, err );
	const double sec = std::chrono::duration<double>( std::chrono::steady_clock::now() - t0 ).count();
	std::printf( "\n" );
	if( !ok ) { std::fprintf( stderr, "ERROR: %s\n", err.c_str() ); return 1; }
	// コマンドラインのベイクは従来どおりそのままファイルへ保存する (GUI では File > Save... で保存)
	if( !ibl::SaveBaked( baked, c.output, out, err ) ) { std::fprintf( stderr, "ERROR: %s\n", err.c_str() ); return 1; }
	std::printf( "Baked on %s in %.2f s\n", c.settings.useGpu ? "GPU" : "CPU", sec );
	std::printf( "Output:\n  %s\n  %s\n  %s\n", util::PathToUtf8( out.prefilteredCube ).c_str(), util::PathToUtf8( out.brdfLut ).c_str(),
				 util::PathToUtf8( out.irradianceCube ).c_str() );
	return 0;
}
