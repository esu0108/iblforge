// AsyncHdrLoader.h : HDRI を別スレッドで読み込み、プレビュー用の縮小画像まで作る (Win32 / D3D12 非依存)
#pragma once
#include <filesystem>
#include <future>
#include <memory>
#include <string>

#include "HdrPreview.h"
#include "IBLBaker.h"

struct HdrLoadResult
{
	bool ok = false;
	std::string err;
	std::filesystem::path path;
	std::shared_ptr<ibl::EquirectImage> image;
	ibl::PreviewSource preview;
};

class AsyncHdrLoader
{
public:
	bool IsLoading() const { return m_future.valid(); }

	// 読み込みを開始する。読み込み中なら false
	bool Start( const std::filesystem::path& path );

	// 完了していれば結果を取り出す (毎フレーム呼んでよい)。未完了 / 読み込み中でないなら false
	bool TryTake( HdrLoadResult& out );

private:
	std::future<HdrLoadResult> m_future; // 読み込み中だけ valid
};
