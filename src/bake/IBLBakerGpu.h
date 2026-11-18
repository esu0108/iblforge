// IBLBakerGpu.h : IBL ベイクの GPU 実装 (Direct3D 12 Compute Shader)
//   shaders/ibl_bake.hlsl をビルド時にコンパイルした .cso (assets/shaders/) を実行時に読み込んで使う。呼び出し側が渡した D3D12 デバイス (ビューアと共有) を使う。
//   nullptr なら専用デバイスを作るので、別スレッド / --bake-only (ウィンドウ無し) からも呼べる。
//   アルゴリズムは IBLBaker.cpp (CPU 版) と同じで、結果は CubeMap / LUT として CPU メモリに読み戻す。
//   → DDS への保存・ビューアへの再読み込みは CPU 版と共通のコードを使う。
#pragma once
#include <string>
#include <vector>

#include "IBLBaker.h"

struct ID3D12Device;

namespace ibl
{
	struct GpuBakeResult
	{
		CubeMap irradiance;
		CubeMap prefiltered;
		std::vector<float> brdfLut; // size*size*2 (R=scale, G=bias)。BakeBrdfLut と同じ並び。
		                            // 呼び出し前に lutSize^2*2 要素の計算済み LUT を入れておくと、GPU での LUT 計算を省略してそのまま返す。
	};

	// s の各サイズは RunBake で正規化済み (2 の累乗) であること。
	// 失敗時 (D3D12 デバイスが無い / シェーダーコンパイル失敗 / GPU ハング等) は false と err を返す。
	// device: 使う D3D12 デバイス (ビューアと共有)。nullptr なら専用のデバイスを作る (--bake-only 用)。
	// progress が BakeCancelled を投げた場合は、そのまま呼び出し元へ伝える (err は設定しない)。
	bool BakeOnGpu( ID3D12Device* device, const EquirectImage& eq, const BakeSettings& s, const ProgressFn& progress, GpuBakeResult& out, std::string& err );
} // namespace ibl
