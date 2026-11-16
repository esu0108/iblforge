// FrameCommands.h : フレームごとのコマンドアロケーターと、フレームの GPU 完了フェンス (トリプルバッファリングの同期)
#pragma once
#include "gfx/GpuContext.h"

#include <array>

class FrameCommands
{
public:
	static constexpr UINT kFrameCount = 3;

	void Init( ID3D12Device* device );
	void Release();

	// frameIndex 番目のフレームを開始する。そのフレームの前回の描画が GPU で終わるまで待ち、コマンドリストをリセットして返す
	ID3D12GraphicsCommandList* Begin( GpuContext& gpu, UINT frameIndex );
	// Close して実行する
	void Execute( GpuContext& gpu );
	// Present 後に呼ぶ。このフレームの完了フェンスを記録する
	void MarkSubmitted( GpuContext& gpu );

	ID3D12GraphicsCommandList* List() const { return m_cmd.Get(); }

private:
	std::array<gpu::ComPtr<ID3D12CommandAllocator>, kFrameCount> m_allocators;
	gpu::ComPtr<ID3D12GraphicsCommandList> m_cmd;
	std::array<UINT64, kFrameCount> m_frameFence{};
	UINT m_frameIndex = 0;
};
