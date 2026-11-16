#include "BakePipelines.h"

#include <d3dcompiler.h>

#include <filesystem>
#include <stdexcept>

namespace ibl
{
	namespace gpu
	{
		namespace
		{
			ComPtr<ID3DBlob> CompileCs( const std::filesystem::path& file, const char* entry )
			{
				UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
				ComPtr<ID3DBlob> code, errors;
				const HRESULT hr = D3DCompileFromFile( file.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, "cs_5_1", flags, 0, &code, &errors );
				if( FAILED( hr ) )
				{
					std::string msg = "Shader compile failed: " + util::PathToUtf8( file ) + " [" + entry + "]";
					if( errors ) msg += "\n" + std::string( static_cast<const char*>( errors->GetBufferPointer() ), errors->GetBufferSize() );
					else msg += " (HRESULT " + util::HrToString( hr ) + ") - shaders フォルダが exe の隣にあるか確認してください";
					throw std::runtime_error( msg );
				}
				return code;
			}

			ComPtr<ID3D12RootSignature> CreateRootSignature( ID3D12Device* dev )
			{
				// ルートシグネチャ: [0] 32bit 定数 (b0, 12 DWORD)  [1] テーブル (SRV t0..t3, UAV u0)  + 静的サンプラ s0
				D3D12_DESCRIPTOR_RANGE ranges[2]{};
				ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				ranges[0].NumDescriptors = 4;
				ranges[0].BaseShaderRegister = 0;
				ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				ranges[1].NumDescriptors = 1;
				ranges[1].BaseShaderRegister = 0;
				ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				D3D12_ROOT_PARAMETER params[2]{};
				params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
				params[0].Constants.ShaderRegister = 0;
				params[0].Constants.Num32BitValues = sizeof( GpuParams ) / 4;
				params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				params[1].DescriptorTable.NumDescriptorRanges = 2;
				params[1].DescriptorTable.pDescriptorRanges = ranges;
				params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

				D3D12_STATIC_SAMPLER_DESC samp{};
				samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				samp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
				samp.MaxAnisotropy = 1;
				samp.MinLOD = 0.0f;
				samp.MaxLOD = D3D12_FLOAT32_MAX;
				samp.ShaderRegister = 0;
				samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

				D3D12_ROOT_SIGNATURE_DESC rs{};
				rs.NumParameters = 2;
				rs.pParameters = params;
				rs.NumStaticSamplers = 1;
				rs.pStaticSamplers = &samp;
				rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
				ComPtr<ID3DBlob> blob, err;
				const HRESULT hr = D3D12SerializeRootSignature( &rs, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err );
				if( FAILED( hr ) ) throw std::runtime_error( std::string( "D3D12SerializeRootSignature: " ) + ( err ? static_cast<const char*>( err->GetBufferPointer() ) : "" ) );
				ComPtr<ID3D12RootSignature> sig;
				ThrowIfFailed( dev->CreateRootSignature( 0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS( &sig ) ), "CreateRootSignature" );
				return sig;
			}
		} // namespace

		BakePipelines::BakePipelines( ID3D12Device* dev )
		{
			rootSig = CreateRootSignature( dev );

			const std::filesystem::path shader = util::ExeDir() / L"shaders" / L"ibl_bake.hlsl";
			auto make = [&]( const char* entry ) {
				ComPtr<ID3DBlob> cs = CompileCs( shader, entry );
				D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
				d.pRootSignature = rootSig.Get();
				d.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };
				ComPtr<ID3D12PipelineState> pso;
				ThrowIfFailed( dev->CreateComputePipelineState( &d, IID_PPV_ARGS( &pso ) ), "CreateComputePipelineState" );
				return pso;
			};
			equirect = make( "EquirectToCubeCS" );
			downsample = make( "DownsampleCS" );
			copy = make( "PrefilterCopyCS" );
			prefilter = make( "PrefilterCS" );
			irradiance = make( "IrradianceCS" );
			lut = make( "BrdfLutCS" );
		}
	} // namespace gpu
} // namespace ibl
