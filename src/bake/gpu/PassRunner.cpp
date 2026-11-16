#include "PassRunner.h"

#include <algorithm>

namespace ibl
{
	namespace gpu
	{
		void StageProgress::Begin( const char* name, uint64_t totalWork )
		{
			m_stage = name;
			m_total = std::max<uint64_t>( totalWork, 1 );
			m_done = 0;
			if( m_progress ) m_progress( m_stage, 0.0f );
		}
		void StageProgress::Report() const
		{
			if( m_progress ) m_progress( m_stage, std::min( 1.0f, static_cast<float>( static_cast<double>( m_done ) / static_cast<double>( m_total ) ) ) );
		}
		void StageProgress::End() const
		{
			if( m_progress ) m_progress( m_stage, 1.0f );
		}

		void PassRunner::Dispatch( ID3D12PipelineState* pso, UINT table, const GpuParams& p, UINT gx, UINT gy, UINT gz, uint64_t work )
		{
			ID3D12GraphicsCommandList* cl = m_ctx.List();
			ID3D12DescriptorHeap* heaps[] = { m_tables.Heap() };
			cl->SetDescriptorHeaps( 1, heaps );
			cl->SetComputeRootSignature( m_pipelines.rootSig.Get() );
			cl->SetPipelineState( pso );
			cl->SetComputeRoot32BitConstants( 0, sizeof( GpuParams ) / 4, &p, 0 );
			cl->SetComputeRootDescriptorTable( 1, m_tables.Gpu( table ) );
			cl->Dispatch( gx, gy, gz );

			m_ctx.AddPending( work );
			m_stage.AddWork( work );
			if( m_ctx.Pending() >= kWorkBudget )
			{
				m_ctx.Flush();
				m_stage.Report();
			}
		}

		void PassRunner::DispatchBanded( ID3D12PipelineState* pso, UINT table, GpuParams p, UINT faces, uint32_t samplesPerPixel )
		{
			const UINT groupsX = DivUp( p.size, kGroup );
			const UINT groupsY = groupsX;
			const uint64_t workPerGroupRow = static_cast<uint64_t>( kGroup ) * ( groupsX * kGroup ) * faces * std::max( samplesPerPixel, 1u );
			const UINT band = static_cast<UINT>( std::clamp<uint64_t>( kWorkBudget / workPerGroupRow, 1, groupsY ) );
			for( UINT gy = 0; gy < groupsY; gy += band )
			{
				const UINT n = std::min( band, groupsY - gy );
				p.rowOffset = gy * kGroup;
				Dispatch( pso, table, p, groupsX, n, faces, workPerGroupRow * n );
			}
		}
	} // namespace gpu
} // namespace ibl
