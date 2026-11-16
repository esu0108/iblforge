// ComputeContext.h : GPU ベイク専用の D3D12 デバイス / コマンドキュー / コマンドリスト / フェンス。
//   「コマンドを積んで Flush で実行 + 完了待ち」だけを担当する (リソース生成・ディスクリプタ・dispatch は別クラス)。
#pragma once
#include <utility>
#include <vector>

#include "GpuCommon.h"

namespace ibl
{
	namespace gpu
	{
		class ComputeContext
		{
		public:
			// device を渡すとそれを共有し (ビューアのデバイス)、nullptr なら高性能 GPU を選んで専用デバイスを作る。
			// コマンドキュー / アロケータ / リスト / フェンスは常に専用 (他スレッドの描画と干渉しない)。
			// デバイス作成に失敗 / 機能不足なら std::runtime_error
			explicit ComputeContext( ID3D12Device* device = nullptr );
			~ComputeContext();
			ComputeContext( const ComputeContext& ) = delete;
			ComputeContext& operator=( const ComputeContext& ) = delete;

			ID3D12Device* Device() const { return m_device.Get(); }
			ID3D12GraphicsCommandList* List() const { return m_cl.Get(); }

			// コマンドリストを実行して GPU 完了を待ち、リストを再度開く。TDR 回避のため重い処理はこまめに呼ぶ。
			void Flush();

			// Flush が終わるまで解放できないリソース (upload バッファなど) を保持する
			void KeepAlive( ComPtr<ID3D12Resource> r ) { m_keepAlive.push_back( std::move( r ) ); }

			// 現在のコマンドリストに積んだ仕事量 (Flush で 0 に戻る)
			uint64_t Pending() const { return m_pending; }
			void AddPending( uint64_t w ) { m_pending += w; }

		private:
			void CreateOwnDevice();

			ComPtr<ID3D12Device> m_device;
			ComPtr<ID3D12CommandQueue> m_queue;
			ComPtr<ID3D12CommandAllocator> m_alloc;
			ComPtr<ID3D12GraphicsCommandList> m_cl;
			ComPtr<ID3D12Fence> m_fence;
			HANDLE m_event = nullptr;
			UINT64 m_fenceValue = 0;
			uint64_t m_pending = 0;
			std::vector<ComPtr<ID3D12Resource>> m_keepAlive;
		};
	} // namespace gpu
} // namespace ibl
