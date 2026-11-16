// GpuContext.h : D3D12 デバイス / コマンドキュー / フェンス (GPU との同期点) の所有者
#pragma once
#include "gfx/GpuUtil.h"

class GpuContext
{
public:
	GpuContext() = default;
	GpuContext( const GpuContext& ) = delete;
	GpuContext& operator=( const GpuContext& ) = delete;
	~GpuContext() { Shutdown(); }

	void Init();      // 高性能 GPU を選んでデバイス / キュー / フェンスを作る。例外を投げる
	void Shutdown();  // GPU の完了を待ってから解放する
	bool Valid() const { return m_device != nullptr; }

	IDXGIFactory4*            Factory() const { return m_factory.Get(); }
	ID3D12Device*             Device() const { return m_device.Get(); }
	ID3D12CommandQueue*       Queue() const { return m_queue.Get(); }

	void   WaitIdle();                 // キューが空になるまで待つ
	UINT64 Signal();                   // フェンスを進めて値を返す (その値に到達 = ここまでの描画が完了)
	void   WaitFor( UINT64 value );    // フェンスが value に到達するまで待つ

private:
	gpu::ComPtr<IDXGIFactory4>      m_factory;
	gpu::ComPtr<ID3D12Device>       m_device;
	gpu::ComPtr<ID3D12CommandQueue> m_queue;
	gpu::ComPtr<ID3D12Fence>        m_fence;
	HANDLE m_event = nullptr;
	UINT64 m_counter = 0;
};
