// RenderTargets.h : スワップチェーン / バックバッファ / 深度 と、バックバッファのパスの開始・終了
//
//   シーンは SceneTargets (オフスクリーン) へ描き、TaaResolver がバックバッファへ書き出す。その上に UI (ImGui) を描く。
#pragma once
#include "gfx/GpuContext.h"

#include <array>

class RenderTargets
{
public:
	static constexpr UINT kFrameCount = 3;                                   // トリプルバッファリング
	static constexpr float kClearColor[4] = { 0.08f, 0.08f, 0.09f, 1.0f };   // 背景 (ガンマ空間の値)

	void Init( GpuContext& gpu, HWND hwnd, UINT width, UINT height ); // 例外を投げる
	void Release();

	void Resize( UINT width, UINT height );

	UINT Width() const { return m_width; }
	UINT Height() const { return m_height; }
	UINT CurrentIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }

	D3D12_CPU_DESCRIPTOR_HANDLE BackBufferRtv( UINT frameIndex ) const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthDsv() const { return m_dsvHeap->GetCPUDescriptorHandleForHeapStart(); }

	// バックバッファのパスを開始する (Present -> RenderTarget のバリア / 全面クリア / 全面のビューポート。RTV + 深度を束ねる)
	void BeginBackBufferPass( ID3D12GraphicsCommandList* cmd, UINT frameIndex );
	// 出力先をバックバッファ (1 枚) + 深度に戻す。TaaResolver が複数ターゲットを束ねた後、UI を描く前に呼ぶ
	void BindForUi( ID3D12GraphicsCommandList* cmd, UINT frameIndex );
	// バックバッファを Present 状態に戻す
	void EndPass( ID3D12GraphicsCommandList* cmd, UINT frameIndex );
	void Present( bool vsync );

private:
	void CreateBackBufferResources();
	void ReleaseBackBufferResources();

	GpuContext* m_gpu = nullptr;
	UINT m_width = 0, m_height = 0;
	gpu::ComPtr<IDXGISwapChain3> m_swapChain;
	gpu::ComPtr<ID3D12DescriptorHeap> m_rtvHeap, m_dsvHeap;
	UINT m_rtvInc = 0;

	std::array<gpu::ComPtr<ID3D12Resource>, kFrameCount> m_backBuffers;
	gpu::ComPtr<ID3D12Resource> m_depth; // シーン描画 (SceneTargets のパス) と UI 描画で共有する深度
};
