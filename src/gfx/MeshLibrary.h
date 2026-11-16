// MeshLibrary.h : 表示用メッシュ (内蔵の球 + ビルド時に作られた .mesh モデル) の GPU バッファ
#pragma once
#include "gfx/GpuUtil.h"
#include "gfx/SceneParams.h"

#include <array>
#include <string>

struct GpuMesh
{
	gpu::ComPtr<ID3D12Resource> vb, ib;
	D3D12_VERTEX_BUFFER_VIEW vbv{};
	D3D12_INDEX_BUFFER_VIEW  ibv{};
	UINT indexCount = 0;
	std::string error;       // 読み込み失敗時の理由 (indexCount == 0)
	bool Valid() const { return indexCount > 0; }
};

class MeshLibrary
{
public:
	// 球を作り、models/ の .mesh を読み込む。モデルの失敗は致命的ではなく、その種類が選べなくなるだけ
	void Init( ID3D12Device* device );
	void Release();

	bool IsAvailable( int object ) const { return object >= 0 && object < SceneObject_Count && m_meshes[object].Valid(); }
	const std::string& Error( int object ) const { return m_meshes[object].error; }
	// 選択されたオブジェクト。読み込めていなければ球にフォールバックする
	const GpuMesh& Get( int object ) const { return IsAvailable( object ) ? m_meshes[object] : m_meshes[SceneObject_Sphere]; }

private:
	std::array<GpuMesh, SceneObject_Count> m_meshes;
};
