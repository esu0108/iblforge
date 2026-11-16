// GpuUtil.h : D3D12 の小さな共通ヘルパー (バリア / ヒーププロパティ / アップロードバッファ) と描画形式の定数
#pragma once
#include "Common.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace gpu
{
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // ガンマ変換はシェーダー側で行う
	constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;

	D3D12_RESOURCE_BARRIER Transition( ID3D12Resource* r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after );
	D3D12_HEAP_PROPERTIES  HeapProps( D3D12_HEAP_TYPE type );
	D3D12_RESOURCE_DESC    BufferDesc( UINT64 size );
	ComPtr<ID3D12Resource> CreateUploadBuffer( ID3D12Device* dev, UINT64 size );
} // namespace gpu
