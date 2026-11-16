// mesh_optimize.h : meshoptimizer (v1.3) による頂点 / インデックスバッファの最適化 (ビルド時ツール meshforge 専用)
#pragma once
#include <string>

#include "mesh/MeshLoader.h" // mesh::MeshData

namespace mesh
{
	// 最適化の前後の指標 (meshoptimizer の analyze 系。GPU に依存しないシミュレーション値)
	struct OptimizeStats
	{
		size_t vertsBefore = 0, vertsAfter = 0;
		float acmrBefore = 0, acmrAfter = 0;           // 頂点キャッシュ: 三角形あたりの頂点変換数 (小さいほど良い。理想は 0.5)
		float overdrawBefore = 0, overdrawAfter = 0;   // オーバードロー: 塗られたピクセル / 見えるピクセル (理想は 1.0)
		float overfetchBefore = 0, overfetchAfter = 0; // 頂点フェッチ: 読まれたバイト / バッファのバイト (理想は 1.0)
	};

	// 手順 (meshoptimizer の推奨順):
	//   1. meshopt_generateVertexRemap       完全に同じ頂点を統合 (+ remap 適用)
	//   2. meshopt_optimizeVertexCache       頂点キャッシュのヒット率が高くなるよう三角形を並べ替え
	//   3. meshopt_optimizeOverdraw          視点によらずオーバードローが減るよう、キャッシュ効率を保ったまま三角形をクラスタ単位で並べ替え
	//   4. meshopt_optimizeVertexFetch       頂点をインデックスの初出順に並べ替え (メモリの読み出しを連続に) + 未使用頂点を除去
	// 三角形の集合 (形状) は変わらない。順序だけが変わる。
	bool OptimizeMesh( MeshData& data, OptimizeStats& stats, std::string& err );
} // namespace mesh
