// IblTextures.h : IBL 用の 3 テクスチャ (Prefiltered Cube / BRDF LUT / Irradiance Cube) と、SrvHeap のシーン用スロットへの登録
#pragma once
#include "gfx/GpuContext.h"
#include "gfx/SrvHeap.h"

#include <filesystem>
#include <string>

namespace ibl { struct BakedTextures; }
namespace DirectX { class ScratchImage; }

class IblTextures
{
public:
	void Init( GpuContext& gpu, SrvHeap& srv ) { m_gpu = &gpu; m_srv = &srv; }
	void Release();

	// ベイク済みファイル (DDS) を読み込み GPU へアップロード。フレーム外で呼ぶこと。
	bool LoadFiles( const std::filesystem::path& prefilteredDds, const std::filesystem::path& lutDds,
					const std::filesystem::path& irradianceDds, std::string& err );
	// ベイク直後のメモリ上の結果をそのままアップロード。ファイルを経由しない。
	bool LoadBaked( const ibl::BakedTextures& baked, std::string& err );

	bool Ready() const { return m_ready; }
	UINT MipCount() const { return m_mipCount; }
	D3D12_GPU_DESCRIPTOR_HANDLE TableGpu() const { return m_srv->Gpu( SrvHeap::kSlotPrefilteredCube ); } // t0..t2 の先頭
	D3D12_GPU_DESCRIPTOR_HANDLE BrdfLutGpu() const { return m_srv->Gpu( SrvHeap::kSlotBrdfLut ); }        // ImGui::Image 用

private:
	bool Upload( const DirectX::ScratchImage& cube, const DirectX::ScratchImage& lut, const DirectX::ScratchImage& irr, std::string& err );

	GpuContext* m_gpu = nullptr;
	SrvHeap* m_srv = nullptr;
	gpu::ComPtr<ID3D12Resource> m_cubeTex, m_lutTex, m_irrTex;
	bool m_ready = false;
	UINT m_mipCount = 1;
};
