// TextureUpload.h : DirectXTex の ScratchImage を GPU テクスチャ (DEFAULT ヒープ, PIXEL_SHADER_RESOURCE 状態) へアップロードする
#pragma once
#include "gfx/GpuContext.h"

namespace DirectX { class ScratchImage; }

// 全サブリソース (mip x 配列) をコピーし、完了を待ってから返す (同期)。例外を投げる。
gpu::ComPtr<ID3D12Resource> UploadTexture( GpuContext& gpu, const DirectX::ScratchImage& scratch );
