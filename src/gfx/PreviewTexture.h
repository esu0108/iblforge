// PreviewTexture.h : 左パネルに表示する HDRI プレビュー (トーンマップ済み RGBA8) の GPU テクスチャ。ImGui::Image 用の SRV を持つ。
#pragma once
#include "gfx/GpuContext.h"
#include "gfx/SrvHeap.h"

#include <cstdint>
#include <string>

class PreviewTexture
{
public:
	void Init( GpuContext& gpu, SrvHeap& srv ) { m_gpu = &gpu; m_srv = &srv; }
	void Release() { m_tex.Reset(); }

	// RGBA8 (行優先) で差し替える。SRV スロットは 1 度だけ確保して使い回す。フレーム外で呼ぶこと。
	bool Set( const uint8_t* rgba, UINT width, UINT height, std::string& err );

	bool Has() const { return m_tex != nullptr; }
	D3D12_GPU_DESCRIPTOR_HANDLE Gpu() const { return m_gpuHandle; }

private:
	GpuContext* m_gpu = nullptr;
	SrvHeap* m_srv = nullptr;
	gpu::ComPtr<ID3D12Resource> m_tex;
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle{};
};
