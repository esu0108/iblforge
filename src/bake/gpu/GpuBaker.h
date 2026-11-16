// GpuBaker.h : GPU ベイクの手順 (どの pass をどの順で回すか) だけを持つ。
//   D3D12 の下回りは ComputeContext / DescriptorTables / BakePipelines / PassRunner / Readback に分離してある。
#pragma once
#include "../IBLBakerGpu.h"
#include "BakePipelines.h"
#include "ComputeContext.h"
#include "DescriptorTables.h"
#include "PassRunner.h"
#include "SampleTableBuffer.h"

namespace ibl
{
	namespace gpu
	{
		class GpuBaker
		{
		public:
			GpuBaker( ID3D12Device* device, const ProgressFn& progress ); // device == nullptr なら専用デバイスを作る

			void Run( const EquirectImage& eq, const BakeSettings& s, GpuBakeResult& out ); // 失敗時 std::exception

		private:
			void EquirectToCube( ID3D12Resource* dst, UINT size );
			void GenerateMips( ID3D12Resource* cube, UINT size, UINT mips );
			ComPtr<ID3D12Resource> Irradiance( ID3D12Resource* srcCube, UINT srcMips, ID3D12Resource* samples, UINT sampleTotal, const SampleTableBuffer& st, UINT size );
			ComPtr<ID3D12Resource> Prefilter( ID3D12Resource* srcCube, UINT srcMips, ID3D12Resource* samples, UINT sampleTotal, const SampleTableBuffer& st, UINT size, UINT mips );
			ComPtr<ID3D12Resource> BrdfLut( UINT size, UINT samples );

			StageProgress m_stage;
			ComputeContext m_ctx;
			BakePipelines m_pipelines;
			DescriptorTables m_tables;
			PassRunner m_runner;

			ComPtr<ID3D12Resource> m_equirect;
			GpuParams m_base;
		};
	} // namespace gpu
} // namespace ibl
