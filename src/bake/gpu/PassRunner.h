// PassRunner.h : コンピュート pass の dispatch (行バンド分割 + TDR 回避の定期 Flush) と、ステージ単位の進捗通知。
#pragma once
#include <string>

#include "../IBLBaker.h"
#include "BakePipelines.h"
#include "ComputeContext.h"
#include "DescriptorTables.h"

namespace ibl
{
	namespace gpu
	{
		// ステージ (= 進捗バーに出る処理名) と仕事量ベースの進捗率
		class StageProgress
		{
		public:
			explicit StageProgress( const ProgressFn& progress ) : m_progress( progress ) {}

			void Begin( const char* name, uint64_t totalWork );
			void AddWork( uint64_t w ) { m_done += w; }
			void Report() const;
			void End() const;
			void Notify( const char* name, float fraction ) const { if( m_progress ) m_progress( name, fraction ); }

		private:
			ProgressFn m_progress;
			const char* m_stage = "";
			uint64_t m_total = 1, m_done = 0;
		};

		class PassRunner
		{
		public:
			PassRunner( ComputeContext& ctx, const BakePipelines& pipelines, DescriptorTables& tables, StageProgress& stage )
				: m_ctx( ctx ), m_pipelines( pipelines ), m_tables( tables ), m_stage( stage ) {}

			// size x size の出力を行バンドに分割して dispatch する。samplesPerPixel は負荷見積もり用。
			void DispatchBanded( ID3D12PipelineState* pso, UINT table, GpuParams p, UINT faces, uint32_t samplesPerPixel );

			static uint64_t PassWork( UINT size, UINT faces, uint32_t samplesPerPixel )
			{
				const uint64_t px = static_cast<uint64_t>( DivUp( size, kGroup ) * kGroup );
				return px * px * faces * samplesPerPixel;
			}

		private:
			void Dispatch( ID3D12PipelineState* pso, UINT table, const GpuParams& p, UINT gx, UINT gy, UINT gz, uint64_t work );

			ComputeContext& m_ctx;
			const BakePipelines& m_pipelines;
			DescriptorTables& m_tables;
			StageProgress& m_stage;
		};
	} // namespace gpu
} // namespace ibl
