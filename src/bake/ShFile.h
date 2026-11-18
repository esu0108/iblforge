// ShFile.h : SH 係数ファイル (<stem>_sh.json) の書き出し / 読み込み。Win32 / D3D 非依存の純 C++
#pragma once
#include <filesystem>
#include <string>

#include "IBLBaker.h"

namespace ibl
{
	// radiance (HDRI を射影した SH) と、そこから求めた irradiance_over_pi (Irradiance Cube と同じ E/π の SH) を JSON で書く。
	// ファイルには基底の並び・座標系・使い方も書き込む (他のエンジンから読めるように)。
	bool WriteShJson( const std::filesystem::path& path, const SH9& radiance, std::string& err );

	// WriteShJson のファイルから radiance を読む ("radiance" の 9 x RGB。irradiance_over_pi は読まず、必要なら IrradianceOverPiSH で作り直す)
	bool ReadShJson( const std::filesystem::path& path, SH9& radiance, std::string& err );
} // namespace ibl
