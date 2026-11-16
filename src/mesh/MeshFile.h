// MeshFile.h : 描画用メッシュのバイナリ形式 (.mesh)。プラットフォーム非依存の純 C++ (rapidobj には依存しない)
//
//   ビルド時に meshforge (tools/meshforge.cpp) が .obj から作り、実行時の Renderer がそのまま頂点/インデックスバッファへ流し込む。
//   .obj のパース・三角形化・法線生成・頂点の共有・正規化・向きの補正は全て変換時に済んでいる。
//
//   ファイルの並び (リトルエンディアン):
//     Header (32 bytes) | Vertex[vertexCount] (24 bytes 各: px py pz nx ny nz の float) | Index[indexCount] (indexSize bytes 各)
//   インデックスは頂点数が 65536 以下なら 16bit (indexSize = 2)、それ以外は 32bit (4)。
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "MeshLoader.h" // mesh::Vertex / mesh::MeshData (ヘッダだけ。rapidobj は MeshLoader.cpp 内に閉じている)

namespace mesh
{
	inline constexpr uint32_t kMeshFileVersion = 1;

	// 読み込み結果。インデックスは GPU にそのまま渡せるバイト列 (indexSize = 2 なら uint16_t, 4 なら uint32_t)
	struct MeshBinary
	{
		std::vector<Vertex>  vertices;
		std::vector<uint8_t> indexData;
		uint32_t indexCount = 0;
		uint32_t indexSize = 4;
	};

	bool SaveMeshFile( const std::filesystem::path& path, const MeshData& data, std::string& err );

	// 壊れたファイル / 形式違いは false と err (UTF-8)。サイズ・インデックスの範囲・三角形の個数を検証する。
	bool LoadMeshFile( const std::filesystem::path& path, MeshBinary& out, std::string& err );
} // namespace mesh
