// meshforge : .obj を実行時用のバイナリメッシュ (.mesh) に変換するビルド時ツール (CMake が呼ぶ)
//
//   meshforge <input.obj> <output.mesh> [--pitch <deg>] [--yaw <deg>] [--no-optimize]
//
//   rapidobj での読み込み・三角形化・法線生成・正規化・向きの補正を済ませ、meshoptimizer で頂点 / インデックスバッファを
//   最適化 (頂点統合 → 頂点キャッシュ → オーバードロー → 頂点フェッチ) したうえで、そのまま保存する。--no-optimize で最適化を省略。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "mesh/MeshFile.h"
#include "mesh/MeshLoader.h"
#include "mesh_optimize.h"

int main( int argc, char** argv )
{
	if( argc < 3 )
	{
		std::fprintf( stderr, "usage: %s <input.obj> <output.mesh> [--pitch <deg>] [--yaw <deg>] [--no-optimize]\n", argv[0] );
		return 2;
	}
	mesh::LoadOptions opt;
	bool optimize = true;
	for( int i = 3; i < argc; ++i )
	{
		if( std::strcmp( argv[i], "--no-optimize" ) == 0 ) { optimize = false; continue; }
		const bool isPitch = std::strcmp( argv[i], "--pitch" ) == 0, isYaw = std::strcmp( argv[i], "--yaw" ) == 0;
		if( ( isPitch || isYaw ) && i + 1 < argc )
		{
			char* end = nullptr;
			const float v = std::strtof( argv[++i], &end );
			if( end == argv[i] || *end != '\0' ) { std::fprintf( stderr, "meshforge: invalid number: %s\n", argv[i] ); return 2; }
			( isPitch ? opt.pitchDegrees : opt.yawDegrees ) = v;
		}
		else { std::fprintf( stderr, "meshforge: unknown or incomplete option: %s\n", argv[i] ); return 2; }
	}

	mesh::MeshData data;
	std::string err;
	if( !mesh::LoadObj( std::filesystem::path( argv[1] ), data, err, opt ) ) { std::fprintf( stderr, "meshforge: %s: %s\n", argv[1], err.c_str() ); return 1; }

	mesh::OptimizeStats st;
	if( optimize && !mesh::OptimizeMesh( data, st, err ) ) { std::fprintf( stderr, "meshforge: optimize failed: %s\n", err.c_str() ); return 1; }

	std::error_code ec;
	const std::filesystem::path out( argv[2] );
	if( out.has_parent_path() ) std::filesystem::create_directories( out.parent_path(), ec );
	if( !mesh::SaveMeshFile( out, data, err ) ) { std::fprintf( stderr, "meshforge: %s\n", err.c_str() ); return 1; }
	std::printf( "meshforge: %s -> %s (%zu vertices, %zu triangles)\n", argv[1], argv[2], data.vertices.size(), data.indices.size() / 3 );
	if( optimize )
		std::printf( "  optimized: vertices %zu -> %zu | cache ACMR %.3f -> %.3f | overdraw %.3f -> %.3f | overfetch %.3f -> %.3f\n",
					 st.vertsBefore, st.vertsAfter, st.acmrBefore, st.acmrAfter, st.overdrawBefore, st.overdrawAfter, st.overfetchBefore, st.overfetchAfter );
	return 0;
}
