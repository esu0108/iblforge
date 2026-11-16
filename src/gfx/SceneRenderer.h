// SceneRenderer.h : シーン (スカイ + PBR オブジェクト) の描画。ルートシグネチャ / PSO / 定数バッファを所有する。
#pragma once
#include "gfx/FrameCommands.h"
#include "gfx/MeshLibrary.h"
#include "gfx/SceneTargets.h"
#include "gfx/SceneParams.h"

class SceneRenderer
{
public:
	static constexpr UINT kFrameCount = FrameCommands::kFrameCount;

	// ルートシグネチャ / シェーダー / PSO / フレームごとの定数バッファを作る
	void Init( ID3D12Device* device );
	void Release();

	struct Frame
	{
		ID3D12GraphicsCommandList* cmd = nullptr;
		UINT index = 0;                 // 現在のフレーム (定数バッファの選択)
		SceneRect rect;                 // 描画先矩形 (バックバッファのピクセル座標)。Valid() であること
		float jitterX = 0.0f, jitterY = 0.0f; // TAA のサブピクセルジッタ (px。-0.5..0.5)。無効なら 0
	};
	struct Resources
	{
		ID3D12DescriptorHeap* srvHeap = nullptr;
		D3D12_GPU_DESCRIPTOR_HANDLE iblTable{};  // t0=Prefiltered, t1=LUT, t2=Irradiance の先頭
		UINT iblMipCount = 1;
	};

	// f.rect (3D View パネルの内側) にだけ描く。呼び出し側が SceneTargets::BeginScenePass を済ませていること。
	// 描き終えたときの (ジッタ無しの) ビュー射影行列を、次フレームの速度計算 (前フレーム行列) のために覚える。
	void Draw( const Frame& f, const Resources& res, const SceneParams& params, const GpuMesh& object );

private:
	void CreateRootSignature( ID3D12Device* device );
	void CreatePipelines( ID3D12Device* device );

	gpu::ComPtr<ID3D12RootSignature> m_rootSig;
	gpu::ComPtr<ID3D12PipelineState> m_psoObject, m_psoSky;
	std::array<gpu::ComPtr<ID3D12Resource>, kFrameCount> m_cb;
	std::array<UINT8*, kFrameCount> m_cbMapped{};
	DirectX::XMFLOAT4X4 m_prevViewProj{}; // 前回描いたときのジッタ無しビュー射影 (行ベクトル規約のまま)
	bool m_hasPrev = false;
};
