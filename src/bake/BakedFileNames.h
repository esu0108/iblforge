// BakedFileNames.h : ベイク結果の 3 ファイル名の規則 (<stem>_prefiltered.dds / _brdf_lut.dds / _irradiance.dds) からの逆引き
//                    プラットフォーム非依存の純 C++ (テスト可能)
#pragma once
#include <filesystem>

#include "IBLIO.h"

namespace ibl
{
	struct DeriveResult
	{
		enum class Status { Ok, NotBakedName, MissingFile };
		Status status = Status::NotBakedName;
		std::filesystem::path missing; // MissingFile のとき、見つからなかったファイル
	};

	// 3 つのうちどれか 1 つのファイルから残りを割り出す。3 つとも存在すれば Ok (out に 3 パスを返す)。拡張子・サフィックスは大文字小文字を区別しない。
	DeriveResult DeriveBakedFiles( const std::filesystem::path& file, BakeOutputs& out );
}
