// TaaResolver.h : TAA (Temporal Anti-Aliasing) のジッタ列と解決パス。
//   毎フレーム: NextJitter() でサブピクセルのずれを決めて投影行列に足し、シーンを描いた後に Resolve() で
//   「今のシーン + 再投影した履歴」をバックバッファ (と次フレームの履歴) へ書く。履歴は 2 枚を交互に使う。
#pragma once
#include "gfx/FrameCommands.h"
#include "gfx/RenderTargets.h"
#include "gfx/SceneParams.h"
#include "gfx/SceneTargets.h"

class TaaResolver
{
public:
	static constexpr UINT kFrameCount = FrameCommands::kFrameCount;

	void Init( ID3D12Device* device );
	void Release();

	// 次のフレームのジッタ (ピクセル単位, -0.5..0.5)。Halton(2,3) の 16 サンプル周期
	void NextJitter( float& jitterX, float& jitterY );

	// 履歴を捨てる (次の Resolve は今のシーンだけを使う)。サイズ変更 / 矩形の変更 / 有効化 / シーンが描かれなかったフレームの後など
	void InvalidateHistory() { m_historyValid = false; }

	struct Pass
	{
		ID3D12GraphicsCommandList* cmd = nullptr;
		UINT frameIndex = 0;
		ID3D12DescriptorHeap* srvHeap = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv{};
		UINT width = 0, height = 0;   // バックバッファの大きさ
		SceneRect rect;               // シーンを描いた矩形
		bool enabled = true;          // false なら単なるコピー (履歴は使わない)
	};
	// シーンカラーをバックバッファの rect へ書き出す。呼び出し後、出力先は (バックバッファ + 履歴) のまま。UI の前に RenderTargets::BindForUi を呼ぶこと。
	void Resolve( const Pass& pass, SceneTargets& targets );

private:
	void CreatePipeline( ID3D12Device* device );

	gpu::ComPtr<ID3D12RootSignature> m_rootSig;
	gpu::ComPtr<ID3D12PipelineState> m_pso;
	std::array<gpu::ComPtr<ID3D12Resource>, kFrameCount> m_cb;
	std::array<UINT8*, kFrameCount> m_cbMapped{};

	UINT m_jitterIndex = 0;
	UINT m_read = 0;               // 履歴として読む側の添字 (書く側は 1 - m_read)
	bool m_historyValid = false;
	SceneRect m_lastRect;          // 履歴を作ったときの矩形 / サイズ。変わったら履歴は無効
	UINT m_lastWidth = 0, m_lastHeight = 0;
};
