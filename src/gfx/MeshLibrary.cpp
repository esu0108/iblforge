#include "gfx/MeshLibrary.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <vector>

#include "mesh/MeshFile.h"

using util::ThrowIfFailed;

// ---- 表示モデルの一覧。.mesh は CMake がビルド時に .obj から変換したバイナリ (頂点 / インデックスをそのまま持つ)。
//      向きの補正 (Y が上で、正面が初期カメラ = -Z 側に向く) は変換時に焼き込み済みで、値は CMakeLists.txt の MODEL_LIST にある。
namespace
{
	struct ModelDesc
	{
		int object;
		const char* uiName;
		const wchar_t* file;     // assets/models/ 内のファイル名 (CMakeLists.txt の MODEL_LIST の名前 + ".mesh" と一致させること)
	};
	const ModelDesc kModels[] = {
		{ SceneObject_Bunny,       "Stanford Bunny", L"stanford-bunny.mesh" },
		{ SceneObject_Armadillo,   "Armadillo",      L"armadillo.mesh" },
		{ SceneObject_HappyBuddha, "Happy Buddha",   L"happy.mesh" },
		{ SceneObject_Lucy,        "Lucy",           L"lucy.mesh" },
	};
	const ModelDesc* FindModelDesc( int object )
	{
		for( const ModelDesc& m : kModels )
			if( m.object == object ) return &m;
		return nullptr;
	}

	using Vertex = mesh::Vertex; // POSITION(float3) + NORMAL(float3)

	// indexSize: 2 = uint16_t, 4 = uint32_t
	GpuMesh CreateMesh( ID3D12Device* device, const void* vertices, UINT vertexCount, const void* indices, UINT indexCount, UINT indexSize = 4 )
	{
		GpuMesh m;
		m.indexCount = indexCount;
		const UINT vbSize = vertexCount * static_cast<UINT>( sizeof( Vertex ) );
		const UINT ibSize = indexCount * indexSize;
		m.vb = gpu::CreateUploadBuffer( device, vbSize ); // 静的メッシュ (最大でも数 MB) なので UPLOAD ヒープをそのまま使う
		m.ib = gpu::CreateUploadBuffer( device, ibSize );
		void* p = nullptr;
		ThrowIfFailed( m.vb->Map( 0, nullptr, &p ), "Map(VB)" );
		std::memcpy( p, vertices, vbSize );
		m.vb->Unmap( 0, nullptr );
		ThrowIfFailed( m.ib->Map( 0, nullptr, &p ), "Map(IB)" );
		std::memcpy( p, indices, ibSize );
		m.ib->Unmap( 0, nullptr );

		m.vbv = { m.vb->GetGPUVirtualAddress(), vbSize, sizeof( Vertex ) };
		m.ibv = { m.ib->GetGPUVirtualAddress(), ibSize, indexSize == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT };
		return m;
	}

	GpuMesh CreateSphere( ID3D12Device* device )
	{
		constexpr UINT rings = 48, segments = 96;
		constexpr float pi = 3.14159265358979f;
		std::vector<Vertex> verts;
		std::vector<uint32_t> idx;
		for( UINT i = 0; i <= rings; ++i )
		{
			const float theta = pi * static_cast<float>( i ) / rings;
			for( UINT j = 0; j <= segments; ++j )
			{
				const float phi = 2.0f * pi * static_cast<float>( j ) / segments;
				const float x = std::sin( theta ) * std::cos( phi ), y = std::cos( theta ), z = std::sin( theta ) * std::sin( phi );
				verts.push_back( { x, y, z, x, y, z } );
			}
		}
		for( UINT i = 0; i < rings; ++i )
			for( UINT j = 0; j < segments; ++j )
			{
				const uint32_t a = i * ( segments + 1 ) + j, b = a + segments + 1;
				idx.insert( idx.end(), { a, b, a + 1, a + 1, b, b + 1 } );
			}
		return CreateMesh( device, verts.data(), static_cast<UINT>( verts.size() ), idx.data(), static_cast<UINT>( idx.size() ) );
	}
} // namespace

const char* SceneObjectName( int object )
{
	if( const ModelDesc* m = FindModelDesc( object ) ) return m->uiName;
	return object == SceneObject_Sphere ? "Sphere" : "?";
}

void MeshLibrary::Init( ID3D12Device* device )
{
	m_meshes[SceneObject_Sphere] = CreateSphere( device ); // 半径 1

	// モデル: ビルド時に作られたバイナリ (.mesh) を読み、そのままバッファへ流し込む
	for( int i = 0; i < SceneObject_Count; ++i )
	{
		const ModelDesc* md = FindModelDesc( i );
		if( !md ) continue; // 球体は上で作成済み
		GpuMesh& m = m_meshes[i];
		const std::filesystem::path path = util::ExeDir() / L"assets" / L"models" / md->file;
		mesh::MeshBinary data;
		std::string err;
		if( mesh::LoadMeshFile( path, data, err ) )
			m = CreateMesh( device, data.vertices.data(), static_cast<UINT>( data.vertices.size() ), data.indexData.data(), data.indexCount, data.indexSize );
		else
			m.error = util::PathToUtf8( path ) + ": " + err;
	}
}

void MeshLibrary::Release()
{
	for( GpuMesh& m : m_meshes ) m = GpuMesh{};
}
