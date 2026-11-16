#include "gfx/IblTextures.h"

#include <DirectXTex.h>

#include "bake/IBLIO.h"
#include "gfx/TextureUpload.h"

using util::ThrowIfFailed;
using namespace DirectX;

void IblTextures::Release()
{
	m_ready = false;
	m_cubeTex.Reset();
	m_lutTex.Reset();
	m_irrTex.Reset();
}

bool IblTextures::LoadFiles( const std::filesystem::path& cubeDds, const std::filesystem::path& lutDds,
							 const std::filesystem::path& irrDds, std::string& err )
{
	try
	{
		ScratchImage cubeImg, lutImg, irrImg;
		ThrowIfFailed( LoadFromDDSFile( cubeDds.c_str(), DDS_FLAGS_NONE, nullptr, cubeImg ), ( "LoadFromDDSFile: " + util::PathToUtf8( cubeDds ) ).c_str() );
		ThrowIfFailed( LoadFromDDSFile( lutDds.c_str(), DDS_FLAGS_NONE, nullptr, lutImg ), ( "LoadFromDDSFile: " + util::PathToUtf8( lutDds ) ).c_str() );
		ThrowIfFailed( LoadFromDDSFile( irrDds.c_str(), DDS_FLAGS_NONE, nullptr, irrImg ), ( "LoadFromDDSFile: " + util::PathToUtf8( irrDds ) ).c_str() );
		return Upload( cubeImg, lutImg, irrImg, err );
	}
	catch( const std::exception& e )
	{
		err = e.what();
		return false;
	}
}

bool IblTextures::LoadBaked( const ibl::BakedTextures& baked, std::string& err )
{
	if( !baked.Valid() ) { err = "There is no baked result."; return false; }
	return Upload( *baked.prefiltered, *baked.lut, *baked.irradiance, err );
}

bool IblTextures::Upload( const ScratchImage& cubeImg, const ScratchImage& lutImg, const ScratchImage& irrImg, std::string& err )
{
	try
	{
		const TexMetadata& cm = cubeImg.GetMetadata();
		const TexMetadata& lm = lutImg.GetMetadata();
		const TexMetadata& im = irrImg.GetMetadata();
		if( !im.IsCubemap() || im.arraySize != 6 ) throw std::runtime_error( "Irradiance DDS is not a cube map." );
		if( !cm.IsCubemap() || cm.arraySize != 6 ) throw std::runtime_error( "Prefiltered DDS is not a cube map." );
		if( lm.dimension != TEX_DIMENSION_TEXTURE2D ) throw std::runtime_error( "BRDF LUT DDS is not a 2D texture." );

		m_gpu->WaitIdle(); // 旧テクスチャを GPU が参照していない状態にする
		m_ready = false;
		m_cubeTex = UploadTexture( *m_gpu, cubeImg );
		m_lutTex = UploadTexture( *m_gpu, lutImg );
		m_irrTex = UploadTexture( *m_gpu, irrImg );

		ID3D12Device* dev = m_gpu->Device();
		D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
		sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sv.Format = cm.format;
		sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		sv.TextureCube.MostDetailedMip = 0;
		sv.TextureCube.MipLevels = static_cast<UINT>( cm.mipLevels );
		sv.TextureCube.ResourceMinLODClamp = 0.0f;
		dev->CreateShaderResourceView( m_cubeTex.Get(), &sv, m_srv->Cpu( SrvHeap::kSlotPrefilteredCube ) );

		D3D12_SHADER_RESOURCE_VIEW_DESC lv{};
		lv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		lv.Format = lm.format;
		lv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		lv.Texture2D.MostDetailedMip = 0;
		lv.Texture2D.MipLevels = 1;
		dev->CreateShaderResourceView( m_lutTex.Get(), &lv, m_srv->Cpu( SrvHeap::kSlotBrdfLut ) );

		D3D12_SHADER_RESOURCE_VIEW_DESC iv = sv;
		iv.Format = im.format;
		iv.TextureCube.MipLevels = static_cast<UINT>( im.mipLevels );
		dev->CreateShaderResourceView( m_irrTex.Get(), &iv, m_srv->Cpu( SrvHeap::kSlotIrradianceCube ) );

		m_mipCount = static_cast<UINT>( cm.mipLevels );
		m_ready = true;
		return true;
	}
	catch( const std::exception& e )
	{
		err = e.what();
		return false;
	}
}
