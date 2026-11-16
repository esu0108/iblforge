// IBLIO.h : DirectXTex を使った HDR 読み込み / DDS 出力、およびベイク全体のパイプライン
#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "IBLBaker.h"

namespace DirectX { class ScratchImage; }
struct ID3D12Device;

namespace ibl
{
	struct BakeJob
	{
		std::shared_ptr<const EquirectImage> hdr; // 読み込み済みの HDR 画像 (LoadEquirectHdr の結果)。ベイクスレッドから読むだけ
		std::wstring stem;                        // 出力ファイル名のもと (通常は HDR のファイル名から拡張子を除いたもの)
		BakeSettings settings;
		ID3D12Device* device = nullptr;           // GPU ベイクで使うデバイス (ビューアと共有)。nullptr なら専用デバイスを作る。ベイク中は生存していること
	};

	// 出力ファイル一式 (入力 HDR のファイル名 stem から決まる)
	struct BakeOutputs
	{
		std::filesystem::path prefilteredCube; // <stem>_prefiltered.dds  (BakeSettings::cubeFormat, Cube, 全mip)
		std::filesystem::path brdfLut;         // <stem>_brdf_lut.dds     (R16G16_FLOAT, 2D)
		std::filesystem::path irradianceCube;  // <stem>_irradiance.dds   (BakeSettings::cubeFormat, Cube, mip 無し, = E/π)

		static BakeOutputs ForStem( const std::filesystem::path& dir, const std::wstring& stem );
	};

	// ベイク結果 (メモリ上)。ファイルに保存される DDS と全く同じ内容 (Cube は settings.cubeFormat、LUT は R16G16_FLOAT) を保持する。
	// ベイクしただけではファイルは作られない。SaveBaked で明示的に保存する。
	struct BakedTextures
	{
		std::shared_ptr<DirectX::ScratchImage> prefiltered; // Cube, 全 mip
		std::shared_ptr<DirectX::ScratchImage> lut;         // 2D
		std::shared_ptr<DirectX::ScratchImage> irradiance;  // Cube, mip 無し
		std::wstring stem;                                  // 入力 HDR のファイル名 (拡張子なし) = 出力ファイル名のもと
		bool Valid() const { return prefiltered && lut && irradiance; }
	};

	// .hdr を読み込む (別スレッドから呼んでよい)
	bool LoadEquirectHdr( const std::filesystem::path& path, EquirectImage& out, std::string& err );

	// 読み込み済み HDRI (job.hdr) に対して各種ベイクを実行し、結果をメモリ上に返す (ファイルは書かない。別スレッドから呼んでよい)。
	// progress が BakeCancelled を投げると中断し、false と err = "Canceled" を返す。
	bool RunBake( const BakeJob& job, const ProgressFn& progress, BakedTextures& out, std::string& err );

	// ベイク結果を dir に DDS 3 ファイルとして保存する (dir は無ければ作成)。保存先は outputs に返す。
	bool SaveBaked( const BakedTextures& baked, const std::filesystem::path& dir, BakeOutputs& outputs, std::string& err );
} // namespace ibl
