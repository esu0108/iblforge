#include "mesh_optimize.h"

#include <meshoptimizer.h>

#include <vector>

namespace mesh
{
	namespace
	{
		static_assert( sizeof( Vertex ) == 24 && offsetof( Vertex, px ) == 0, "position must be the first 12 bytes (meshopt_optimizeOverdraw)" );

		void Measure( const MeshData& d, float& acmr, float& overdraw, float& overfetch )
		{
			const size_t ni = d.indices.size(), nv = d.vertices.size();
			// キャッシュ: 16 エントリ FIFO の一般的な設定 (warp 0 / primgroup 0 = 無効)
			acmr = meshopt_analyzeVertexCache( d.indices.data(), ni, nv, 16, 0, 0 ).acmr;
			overdraw = meshopt_analyzeOverdraw( d.indices.data(), ni, &d.vertices[0].px, nv, sizeof( Vertex ) ).overdraw;
			overfetch = meshopt_analyzeVertexFetch( d.indices.data(), ni, nv, sizeof( Vertex ) ).overfetch;
		}
	} // namespace

	bool OptimizeMesh( MeshData& data, OptimizeStats& stats, std::string& err )
	{
		const size_t indexCount = data.indices.size();
		if( data.vertices.empty() || indexCount == 0 || indexCount % 3 != 0 ) { err = "The mesh is empty or not a triangle list."; return false; }
		for( const uint32_t i : data.indices )
			if( i >= data.vertices.size() ) { err = "The mesh has an out-of-range index."; return false; }

		stats = {};
		stats.vertsBefore = data.vertices.size();
		Measure( data, stats.acmrBefore, stats.overdrawBefore, stats.overfetchBefore );

		// 1. 同一頂点の統合 (位置 + 法線が完全に同じもの)
		{
			std::vector<unsigned int> remap( data.vertices.size() );
			const size_t unique = meshopt_generateVertexRemap( remap.data(), data.indices.data(), indexCount, data.vertices.data(), data.vertices.size(), sizeof( Vertex ) );
			std::vector<uint32_t> newIndices( indexCount );
			std::vector<Vertex> newVertices( unique );
			meshopt_remapIndexBuffer( newIndices.data(), data.indices.data(), indexCount, remap.data() );
			meshopt_remapVertexBuffer( newVertices.data(), data.vertices.data(), data.vertices.size(), sizeof( Vertex ), remap.data() );
			data.indices = std::move( newIndices );
			data.vertices = std::move( newVertices );
		}
		const size_t vertexCount = data.vertices.size();

		// 2. 頂点キャッシュ最適化 (in-place 可)
		meshopt_optimizeVertexCache( data.indices.data(), data.indices.data(), indexCount, vertexCount );

		// 3. オーバードロー最適化 (2 の結果が入力であること。threshold 1.05 = キャッシュ効率の劣化を最大 5% まで許容)
		meshopt_optimizeOverdraw( data.indices.data(), data.indices.data(), indexCount, &data.vertices[0].px, vertexCount, sizeof( Vertex ), 1.05f );

		// 4. 頂点フェッチ最適化 (頂点バッファを並べ替え、インデックスを書き換える。戻り値 = 使われている頂点数)
		const size_t used = meshopt_optimizeVertexFetch( data.vertices.data(), data.indices.data(), indexCount, data.vertices.data(), vertexCount, sizeof( Vertex ) );
		data.vertices.resize( used );

		stats.vertsAfter = data.vertices.size();
		Measure( data, stats.acmrAfter, stats.overdrawAfter, stats.overfetchAfter );
		return true;
	}
} // namespace mesh
