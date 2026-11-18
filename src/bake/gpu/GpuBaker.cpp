#include "GpuBaker.h"

#include <algorithm>

#include "GpuResources.h"
#include "Readback.h"

namespace ibl
{
	namespace gpu
	{
		GpuBaker::GpuBaker( ID3D12Device* device, const ProgressFn& progress )
			: m_stage( progress ), m_ctx( device ), m_pipelines( m_ctx.Device() ), m_tables( m_ctx.Device() ),
			  m_runner( m_ctx, m_pipelines, m_tables, m_stage )
		{
		}

		void GpuBaker::EquirectToCube( ID3D12Resource* dst, UINT size )
		{
			m_stage.Begin( "Equirect -> Cube (GPU)", PassRunner::PassWork( size, 6, 4 ) );
			const UINT t = m_tables.Alloc();
			m_tables.SrvEquirect( t, m_equirect.Get() );
			m_tables.Uav( t, dst, 0, 6 );
			GpuParams p = m_base;
			p.size = size;
			m_runner.DispatchBanded( m_pipelines.equirect.Get(), t, p, 6, 4 );
			m_ctx.Flush();
			m_stage.End();
		}

		void GpuBaker::GenerateMips( ID3D12Resource* cube, UINT size, UINT mips )
		{
			uint64_t total = 0;
			for( UINT m = 1; m < mips; ++m ) total += PassRunner::PassWork( std::max( size >> m, 1u ), 6, 1 );
			m_stage.Begin( "Generating mips (GPU)", total );
			for( UINT m = 1; m < mips; ++m )
			{
				TransitionAllMipSlices( m_ctx.List(), cube, m - 1, mips, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
				const UINT t = m_tables.Alloc();
				m_tables.SrvMipArray( t, cube, m - 1 );
				m_tables.Uav( t, cube, m, 6 );
				GpuParams p = m_base;
				p.size = std::max( size >> m, 1u );
				m_runner.DispatchBanded( m_pipelines.downsample.Get(), t, p, 6, 1 );
			}
			TransitionAllMipSlices( m_ctx.List(), cube, mips - 1, mips, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
			m_ctx.Flush();
			m_stage.End();
		}

		ComPtr<ID3D12Resource> GpuBaker::Irradiance( ID3D12Resource* srcCube, UINT srcMips, ID3D12Resource* samples, UINT sampleTotal, const SampleTableBuffer& st, UINT size )
		{
			ComPtr<ID3D12Resource> tex = CreateTexture( m_ctx.Device(), size, 6, 1, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
			m_stage.Begin( "Irradiance (diffuse) (GPU)", PassRunner::PassWork( size, 6, st.irrCount ) );
			const UINT t = m_tables.Alloc();
			m_tables.SrvCube( t, srcCube, srcMips );
			m_tables.SrvSamples( t, samples, sampleTotal );
			m_tables.Uav( t, tex.Get(), 0, 6 );
			GpuParams p = m_base;
			p.size = size;
			p.sampleCount = st.irrCount;
			p.sampleOffset = st.irrOffset;
			m_runner.DispatchBanded( m_pipelines.irradiance.Get(), t, p, 6, st.irrCount );
			m_ctx.Flush();
			m_stage.End();
			return tex;
		}

		ComPtr<ID3D12Resource> GpuBaker::Prefilter( ID3D12Resource* srcCube, UINT srcMips, ID3D12Resource* samples, UINT sampleTotal, const SampleTableBuffer& st, UINT size, UINT mips )
		{
			ComPtr<ID3D12Resource> tex = CreateTexture( m_ctx.Device(), size, 6, mips, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
			uint64_t total = 0;
			for( UINT m = 0; m < mips; ++m ) total += PassRunner::PassWork( std::max( size >> m, 1u ), 6, m == 0 ? 1 : st.pfCount[m] );
			m_stage.Begin( "Prefilter specular (GPU)", total );
			for( UINT m = 0; m < mips; ++m )
			{
				const UINT t = m_tables.Alloc();
				m_tables.SrvCube( t, srcCube, srcMips );
				m_tables.SrvSamples( t, samples, sampleTotal );
				m_tables.Uav( t, tex.Get(), m, 6 );
				GpuParams p = m_base;
				p.size = std::max( size >> m, 1u );
				p.sampleCount = st.pfCount[m];
				p.sampleOffset = st.pfOffset[m];
				p.roughness = mips > 1 ? static_cast<float>( m ) / static_cast<float>( mips - 1 ) : 0.0f;
				m_runner.DispatchBanded( m == 0 ? m_pipelines.copy.Get() : m_pipelines.prefilter.Get(), t, p, 6, m == 0 ? 1 : st.pfCount[m] );
			}
			m_ctx.Flush();
			m_stage.End();
			return tex;
		}

		ComPtr<ID3D12Resource> GpuBaker::BrdfLut( UINT size, UINT samples )
		{
			ComPtr<ID3D12Resource> tex = CreateTexture( m_ctx.Device(), size, 1, 1, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
			m_stage.Begin( "BRDF LUT (GPU)", PassRunner::PassWork( size, 1, samples ) );
			const UINT t = m_tables.Alloc();
			m_tables.Uav( t, tex.Get(), 0, 1 );
			GpuParams p = m_base;
			p.size = size;
			p.sampleCount = samples;
			m_runner.DispatchBanded( m_pipelines.lut.Get(), t, p, 1, samples );
			m_ctx.Flush();
			m_stage.End();
			return tex;
		}

		void GpuBaker::Run( const EquirectImage& eq, const BakeSettings& s, GpuBakeResult& out )
		{
			m_stage.Notify( "Uploading HDRI (GPU)", 0.0f );
			m_equirect = UploadEquirect( m_ctx, eq );

			m_base = GpuParams{};
			m_base.eqWidth = eq.width;
			m_base.eqHeight = eq.height;
			m_base.maxSample = s.maxSampleValue;

			// ---------------- ソースキューブ (Equirect -> Cube + mip チェーン)
			const UINT srcSize = s.sourceCubeSize;
			const UINT srcMips = FloorLog2( srcSize ) + 1;
			ComPtr<ID3D12Resource> srcCube = CreateTexture( m_ctx.Device(), srcSize, 6, srcMips, true, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
			EquirectToCube( srcCube.Get(), srcSize );
			GenerateMips( srcCube.Get(), srcSize, srcMips );
			m_equirect.Reset(); // もう不要 (Flush 済み)

			// ---------------- サンプルテーブル (CPU 版と同一のもの)
			const UINT pfSize = s.prefilterSize;
			const UINT pfMips = FloorLog2( pfSize ) + 1;
			const SampleTableBuffer st = SampleTableBuffer::Build( s, srcSize, srcMips, pfMips );
			ComPtr<ID3D12Resource> sampleBuf = UploadSamples( m_ctx, st.data );
			const UINT sampleTotal = static_cast<UINT>( st.data.size() );

			ComPtr<ID3D12Resource> irrTex = Irradiance( srcCube.Get(), srcMips, sampleBuf.Get(), sampleTotal, st, s.irradianceSize );
			ComPtr<ID3D12Resource> pfTex = Prefilter( srcCube.Get(), srcMips, sampleBuf.Get(), sampleTotal, st, pfSize, pfMips );
			// BRDF LUT は HDRI に依存しない。呼び出し側が計算済みの LUT を out.brdfLut に入れて渡した場合は、GPU 計算と読み戻しを省く。
			const bool lutCached = out.brdfLut.size() == static_cast<size_t>( s.lutSize ) * s.lutSize * 2;
			ComPtr<ID3D12Resource> lutTex;
			if( !lutCached ) lutTex = BrdfLut( s.lutSize, s.lutSamples );

			// ---------------- 読み戻し
			m_stage.Notify( "Reading back (GPU)", 0.0f );
			out.irradiance = ReadbackCube( m_ctx, irrTex.Get(), s.irradianceSize, 1 );
			m_stage.Notify( "Reading back (GPU)", 0.4f );
			out.prefiltered = ReadbackCube( m_ctx, pfTex.Get(), pfSize, pfMips );
			m_stage.Notify( "Reading back (GPU)", 0.9f );
			if( !lutCached ) out.brdfLut = ReadbackLut( m_ctx, lutTex.Get(), s.lutSize );
			m_stage.Notify( "Reading back (GPU)", 1.0f );
		}
	} // namespace gpu
} // namespace ibl
