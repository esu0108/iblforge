#include "gfx/FrameCommands.h"

using util::ThrowIfFailed;

void FrameCommands::Init( ID3D12Device* device )
{
	for( UINT i = 0; i < kFrameCount; ++i )
		ThrowIfFailed( device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &m_allocators[i] ) ), "CreateCommandAllocator" );
	ThrowIfFailed( device->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocators[0].Get(), nullptr, IID_PPV_ARGS( &m_cmd ) ), "CreateCommandList" );
	m_cmd->Close();
}

void FrameCommands::Release()
{
	m_cmd.Reset();
	for( auto& a : m_allocators ) a.Reset();
}

ID3D12GraphicsCommandList* FrameCommands::Begin( GpuContext& gpu, UINT frameIndex )
{
	m_frameIndex = frameIndex;
	gpu.WaitFor( m_frameFence[frameIndex] );
	ThrowIfFailed( m_allocators[frameIndex]->Reset(), "Allocator reset" );
	ThrowIfFailed( m_cmd->Reset( m_allocators[frameIndex].Get(), nullptr ), "CommandList reset" );
	return m_cmd.Get();
}

void FrameCommands::Execute( GpuContext& gpu )
{
	ThrowIfFailed( m_cmd->Close(), "CommandList close" );
	ID3D12CommandList* lists[] = { m_cmd.Get() };
	gpu.Queue()->ExecuteCommandLists( 1, lists );
}

void FrameCommands::MarkSubmitted( GpuContext& gpu )
{
	m_frameFence[m_frameIndex] = gpu.Signal();
}
