// FileDialogs.h : Windows 標準のファイル / フォルダ選択ダイアログ
#pragma once
#include "Common.h"

#include <filesystem>
#include <optional>

// フォルダ選択ダイアログ (IFileOpenDialog + FOS_PICKFOLDERS)。キャンセルなら nullopt。
std::optional<std::filesystem::path> PickFolder( HWND owner, const std::filesystem::path& initial, const wchar_t* title );

// ファイルを開くダイアログ。filter は "説明\0*.ext\0...\0" 形式 (ダブルヌル終端)。キャンセルなら nullopt。
std::optional<std::filesystem::path> PickOpenFile( HWND owner, const wchar_t* title, const wchar_t* filter );
