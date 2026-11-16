// CommandLine.h : コマンドライン引数の解析
//
//   iblforge.exe [input.hdr] [outputDir] [options]
//     --src N          中間キューブの一辺 (既定 512)
//     --size N         Prefiltered Cube mip0 の一辺 (既定 256)
//     --samples N      Prefilter のサンプル数 (既定 1024)
//     --lut N          BRDF LUT の一辺 (既定 256)
//     --lut-samples N  BRDF LUT のサンプル数 (既定 1024)
//     --irr N          Irradiance Cube の一辺 (既定 32)
//     --irr-samples N  Irradiance のサンプル数 (既定 2048)
//     --clamp F        1 サンプルの最大値クランプ (0=無効)
//     --format f16|f32|r11g11b10|bc6h  Cubemap の出力形式 (既定 f16)
//     --gpu / --cpu    ベイクを GPU (Direct3D 12 Compute) / CPU で実行 (既定 GPU)
//     --bake-only      ウィンドウを開かずベイクしてファイル出力のみ行う
#pragma once
#include <string>

#include "bake/IBLBaker.h"

struct CmdLine
{
	std::wstring input;
	std::wstring output = L"output";
	ibl::BakeSettings settings;
	bool bakeOnly = false;
};

CmdLine ParseCmdLine( int argc, wchar_t** argv );
