// SceneTargets.h : シーンのオフスクリーンターゲット (カラー / 速度) と TAA の履歴テクスチャ 2 枚。ウィンドウサイズに追従する。
//   シーンは「カラー (RGBA16F, トーンマップ + sRGB エンコード済み) + 速度 (RG16F, ピクセル単位の再投影ベクトル)」の 2 枚へ描く。
//   どのテクスチャも、パスの外では PIXEL_SHADER_RESOURCE 状態にある。
#pragma once
#include "gfx/GpuUtil.h"
#include "gfx/SrvHeap.h"

class SceneTargets
{
public:
	static constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT kVelocityFormat = DXGI_FORMAT_R16G16_FLOAT;

	void Init( ID3D12Device* device, SrvHeap& srv );
	void Release();
	UINT Width() const { return m_width; }
	UINT Height() const { return m_height; }
	void Resize( UINT width, UINT height ); // 呼び出し側が GPU の完了を待ってから。内容は未初期化になる (履歴は無効)

	// シーンのパス: カラー / 速度を RenderTarget にしてクリアし、深度 (dsv) もクリアして束ねる
	void BeginScenePass( ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE dsv );
	void EndScenePass( ID3D12GraphicsCommandList* cmd );

	// TAA の入力 (t0 = シーン, t1 = 速度, t2 = 履歴[historyIndex])
	D3D12_GPU_DESCRIPTOR_HANDLE InputTable( UINT historyIndex ) const;
	ID3D12Resource* History( UINT i ) const { return m_history[i].Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE HistoryRtv( UINT i ) const { return Rtv( 2 + i ); }

private:
	gpu::ComPtr<ID3D12Resource> CreateTarget( DXGI_FORMAT format, UINT width, UINT height, const float clear[4] ) const;
	D3D12_CPU_DESCRIPTOR_HANDLE Rtv( UINT i ) const;

	ID3D12Device* m_device = nullptr;
	SrvHeap* m_srv = nullptr;
	gpu::ComPtr<ID3D12DescriptorHeap> m_rtvHeap; // [0] = カラー, [1] = 速度, [2..3] = 履歴
	UINT m_rtvInc = 0;
	UINT m_width = 0, m_height = 0;
	gpu::ComPtr<ID3D12Resource> m_color, m_velocity;
	gpu::ComPtr<ID3D12Resource> m_history[2];
};
