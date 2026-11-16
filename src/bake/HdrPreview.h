// HdrPreview.h : 読み込んだ HDRI を左パネルに表示するためのプレビュー (プラットフォーム非依存の純 C++)
#pragma once
#include <cstdint>
#include <vector>

#include "IBLBaker.h"

namespace ibl
{
	// 表示用に縮小した float RGB を持つ。露出を変えて何度でもトーンマップし直せる。
	struct PreviewSource
	{
		uint32_t w = 0, h = 0;
		std::vector<float> rgb;  // w*h*3, リニア
		float autoEV = 0.0f;     // 平均輝度 (対数平均) が中間グレーになる露出
	};

	// 整数倍率の箱型平均で maxWidth 以下に縮小し、自動露出を求める
	PreviewSource BuildPreviewSource( const EquirectImage& eq, uint32_t maxWidth = 1024 );

	// 露出 (EV) → ACES (Narkowicz) → ガンマ → RGBA8。表示専用で、ベイク結果には影響しない
	void TonemapPreview( const PreviewSource& p, float ev, std::vector<uint8_t>& rgba );
} // namespace ibl
