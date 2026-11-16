#include "gfx/PreviewTexture.h"

#include <DirectXTex.h>

#include <cstring>

#include "gfx/TextureUpload.h"

using util::ThrowIfFailed;
using namespace DirectX;

bool PreviewTexture::Set( const uint8_t* rgba, UINT width, UINT height, std::string& err )
{
	try
	{
		if( !rgba || width == 0 || height == 0 ) throw std::runtime_error( "Invalid preview image." );
		ScratchImage si;
		ThrowIfFailed( si.Initialize2D( DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1 ), "Preview ScratchImage" );
		const Image* img = si.GetImage( 0, 0, 0 );
		for( UINT y = 0; y < height; ++y )
			std::memcpy( img->pixels + static_cast<size_t>( y ) * img->rowPitch, rgba + static_cast<size_t>( y ) * width * 4, static_cast<size_t>( width ) * 4 );

		m_gpu->WaitIdle(); // 旧テクスチャを GPU が参照していない状態にする (ImGui の描画が使っている可能性がある)
		m_tex.Reset();
		m_tex = UploadTexture( *m_gpu, si );
		if( m_cpuHandle.ptr == 0 ) m_srv->AllocDynamic( &m_cpuHandle, &m_gpuHandle ); // 1 度だけ確保して使い回す
		D3D12_SHADER_RESOURCE_VIEW_DESC sv{};
		sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		sv.Texture2D.MipLevels = 1;
		m_gpu->Device()->CreateShaderResourceView( m_tex.Get(), &sv, m_cpuHandle );
		return true;
	}
	catch( const std::exception& e )
	{
		err = e.what();
		return false;
	}
}
