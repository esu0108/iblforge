// SrvHeap.h : シェーダーから見える CBV/SRV/UAV ヒープと、そのスロット管理
//   [0..kSceneSlots) = シーン用 (0=Prefiltered cube, 1=BRDF LUT, 2=Irradiance cube) 固定。
//   [kSceneSlots..kReservedSlots) = TAA の入力テーブル 2 本 (A = シーン + 速度 + 履歴 0, B = シーン + 速度 + 履歴 1。履歴は毎フレーム入れ替わる) 固定。
//   それ以降は ImGui (フォントやユーザーテクスチャ) 用の動的スロットで、解放されたものは空きリストで再利用する。
#pragma once
#include "gfx/GpuUtil.h"

#include <vector>

class SrvHeap
{
public:
	static constexpr UINT kSize = 64;
	static constexpr UINT kSceneSlots = 3;
	static constexpr UINT kSlotPrefilteredCube = 0, kSlotBrdfLut = 1, kSlotIrradianceCube = 2;
	static constexpr UINT kTaaTableSize = 3;                   // t0 = シーン, t1 = 速度, t2 = 履歴
	static constexpr UINT kSlotTaaTableA = kSceneSlots;        // 履歴 0 を読むテーブル
	static constexpr UINT kSlotTaaTableB = kSceneSlots + kTaaTableSize;
	static constexpr UINT kReservedSlots = kSceneSlots + 2 * kTaaTableSize;

	void Init( ID3D12Device* device );

	ID3D12DescriptorHeap* Heap() const { return m_heap.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE Cpu( UINT index ) const;
	D3D12_GPU_DESCRIPTOR_HANDLE Gpu( UINT index ) const;

	// 動的スロット (ImGui)。ImGui 1.92 以降はフォントアトラスが拡張・再生成されるたびに SRV を確保 → 古いものを解放するので、空きを再利用する
	void AllocDynamic( D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu );
	void FreeDynamic( D3D12_CPU_DESCRIPTOR_HANDLE cpu );

private:
	gpu::ComPtr<ID3D12DescriptorHeap> m_heap;
	UINT m_inc = 0;
	UINT m_nextDynamic = kReservedSlots;
	std::vector<UINT> m_freeDynamic;
};
