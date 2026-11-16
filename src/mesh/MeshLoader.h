// MeshLoader.h : .obj の読み込み (rapidobj) と描画用頂点バッファへの変換 (プラットフォーム非依存の純C++)
//
//   - 三角形化し、(position, normal) の組で頂点を共有したインデックス付きメッシュにする
//   - 法線が無い .obj (Stanford Bunny など) は面積重み付きのスムース法線を生成する
//   - 原点が中心・外接球の半径が 1 になるよう正規化する (球体と同じカメラ距離で見えるように)
//   - 右手系 (OBJ) -> 左手系 (D3D) 変換として Z を反転する。
//     OBJ の CCW 面は、Z 反転後は D3D の既定の表面 (CW) と一致するのでインデックス順はそのまま。
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mesh
{
	struct Vertex
	{
		float px, py, pz;
		float nx, ny, nz;
	};

	struct MeshData
	{
		std::vector<Vertex>   vertices;
		std::vector<uint32_t> indices; // 三角形リスト
	};

	struct LoadOptions
	{
		// Y 軸まわりの追加回転 (度)。Z 反転後の座標系 (D3D, 左手系) で XMMatrixRotationY と同じ向き。
		// モデルの「正面」を初期カメラ (-Z 側から +Z 方向を見る) に向けるために使う。
		float yawDegrees = 0.0f;
		// X 軸まわりの追加回転 (度)。Z 反転後の座標系で、yaw より先に適用する。(y, z) -> (y*cos - z*sin, y*sin + z*cos)。
		// 元データが Z-up (Lucy など) のとき、Y-up に直すために使う (+90 で Z 反転後の -Z が +Y になる)。
		float pitchDegrees = 0.0f;
	};

	// 失敗時は false と err (UTF-8) を返す。
	bool LoadObj( const std::filesystem::path& path, MeshData& out, std::string& err, const LoadOptions& options = {} );
} // namespace mesh
