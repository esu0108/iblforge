#include "GpuResources.h"

#include <algorithm>
#include <cstring>

namespace ibl
{
	namespace gpu
	{
		ComPtr<ID3D12Resource> CreateTexture( ID3D12Device* dev, UINT size, UINT arraySize, UINT mips, bool uav, D3D12_RESOURCE_STATES state )
		{
			D3D12_RESOURCE_DESC d{};
			d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			d.Width = size;
			d.Height = size;
			d.DepthOrArraySize = static_cast<UINT16>( arraySize );
			d.MipLevels = static_cast<UINT16>( mips );
			d.Format = kFormat;
			d.SampleDesc.Count = 1;
			d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			d.Flags = uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
			const auto heap = HeapProps( D3D12_HEAP_TYPE_DEFAULT );
			ComPtr<ID3D12Resource> r;
			ThrowIfFailed( dev->CreateCommittedResource( &heap, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS( &r ) ), "CreateCommittedResource(texture)" );
			return r;
		}

		ComPtr<ID3D12Resource> CreateBuffer( ID3D12Device* dev, UINT64 size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state )
		{
			D3D12_RESOURCE_DESC d{};
			d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			d.Width = std::max<UINT64>( size, 16 );
			d.Height = 1;
			d.DepthOrArraySize = 1;
			d.MipLevels = 1;
			d.Format = DXGI_FORMAT_UNKNOWN;
			d.SampleDesc.Count = 1;
			d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			const auto heap = HeapProps( heapType );
			ComPtr<ID3D12Resource> r;
			ThrowIfFailed( dev->CreateCommittedResource( &heap, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS( &r ) ), "CreateCommittedResource(buffer)" );
			return r;
		}

		ComPtr<ID3D12Resource> UploadEquirect( ComputeContext& ctx, const EquirectImage& eq )
		{
			ID3D12Device* dev = ctx.Device();
			ID3D12GraphicsCommandList* cl = ctx.List();

			D3D12_RESOURCE_DESC d{};
			d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			d.Width = eq.width;
			d.Height = eq.height;
			d.DepthOrArraySize = 1;
			d.MipLevels = 1;
			d.Format = kFormat;
			d.SampleDesc.Count = 1;
			d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			const auto heap = HeapProps( D3D12_HEAP_TYPE_DEFAULT );
			ComPtr<ID3D12Resource> tex;
			ThrowIfFailed( dev->CreateCommittedResource( &heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS( &tex ) ), "CreateCommittedResource(equirect)" );

			// 巨大な HDRI でも upload バッファが肥大しないよう、行バンドに分けて転送する
			const UINT rowPitch = AlignUp( eq.width * 16, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT );
			const UINT bandRows = std::max<UINT>( 1, std::min<UINT>( eq.height, static_cast<UINT>( ( 64ull * 1024 * 1024 ) / rowPitch ) ) );
			ComPtr<ID3D12Resource> upload = CreateBuffer( dev, static_cast<UINT64>( rowPitch ) * bandRows, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ );
			UINT8* mapped = nullptr;
			ThrowIfFailed( upload->Map( 0, nullptr, reinterpret_cast<void**>( &mapped ) ), "Map(upload)" );

			for( UINT y0 = 0; y0 < eq.height; y0 += bandRows )
			{
				const UINT rows = std::min( bandRows, eq.height - y0 );
				for( UINT r = 0; r < rows; ++r )
				{
					float* dst = reinterpret_cast<float*>( mapped + static_cast<size_t>( r ) * rowPitch );
					const Vec3* src = &eq.pixels[static_cast<size_t>( y0 + r ) * eq.width];
					for( UINT x = 0; x < eq.width; ++x )
					{
						dst[x * 4 + 0] = src[x].x;
						dst[x * 4 + 1] = src[x].y;
						dst[x * 4 + 2] = src[x].z;
						dst[x * 4 + 3] = 1.0f;
					}
				}
				D3D12_TEXTURE_COPY_LOCATION dl{}, sl{};
				dl.pResource = tex.Get();
				dl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dl.SubresourceIndex = 0;
				sl.pResource = upload.Get();
				sl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				sl.PlacedFootprint.Offset = 0;
				sl.PlacedFootprint.Footprint = { kFormat, eq.width, rows, 1, rowPitch };
				cl->CopyTextureRegion( &dl, 0, y0, 0, &sl, nullptr );
				ctx.Flush(); // upload バッファを再利用するので完了を待つ
			}
			upload->Unmap( 0, nullptr );

			const auto b = TransitionBarrier( tex.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
			cl->ResourceBarrier( 1, &b );
			return tex;
		}

		ComPtr<ID3D12Resource> UploadSamples( ComputeContext& ctx, const std::vector<Float4>& data )
		{
			const UINT64 bytes = std::max<size_t>( data.size(), 1 ) * sizeof( Float4 );
			ComPtr<ID3D12Resource> gpu = CreateBuffer( ctx.Device(), bytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON ); // バッファは常に COMMON で作られる (COPY_DEST を指定すると警告)。最初のコピーで暗黙に COPY_DEST へ昇格する
			ComPtr<ID3D12Resource> up = CreateBuffer( ctx.Device(), bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ );
			void* p = nullptr;
			ThrowIfFailed( up->Map( 0, nullptr, &p ), "Map(sample upload)" );
			std::memset( p, 0, static_cast<size_t>( bytes ) );
			if( !data.empty() ) std::memcpy( p, data.data(), data.size() * sizeof( Float4 ) );
			up->Unmap( 0, nullptr );
			ctx.List()->CopyBufferRegion( gpu.Get(), 0, up.Get(), 0, bytes );
			const auto b = TransitionBarrier( gpu.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
			ctx.List()->ResourceBarrier( 1, &b );
			ctx.KeepAlive( up );
			return gpu;
		}

		void TransitionAllMipSlices( ID3D12GraphicsCommandList* cl, ID3D12Resource* r, UINT mip, UINT mipCount, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after )
		{
			D3D12_RESOURCE_BARRIER b[6];
			for( UINT f = 0; f < 6; ++f ) b[f] = TransitionBarrier( r, mip + f * mipCount, before, after );
			cl->ResourceBarrier( 6, b );
		}
	} // namespace gpu
} // namespace ibl
