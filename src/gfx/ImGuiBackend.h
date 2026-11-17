// ImGuiBackend.h : Dear ImGui の初期化 (コンテキスト / フォント / Win32 + DX12 バックエンド) とフレームの開始・描画
#pragma once
#include "gfx/GpuContext.h"
#include "gfx/SrvHeap.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "i18n/I18n.h"

struct ImFont;

class ImGuiBackend
{
public:
	~ImGuiBackend() { Shutdown(); }

	// GPU / SrvHeap が初期化済みであること。DPI スケールでスタイルとフォントを拡大する
	void Init( HWND hwnd, float dpiScale, GpuContext& gpu, SrvHeap& srv, UINT frameCount );
	void Shutdown(); // 呼ぶ前に GPU の完了を待っておくこと

	void NewFrame();                                // ImGui::NewFrame まで行う (言語が変わっていればフォントを切り替える)
	void Render( ID3D12GraphicsCommandList* cmd );  // ImGui::Render + 描画コマンドの記録 (SRV ヒープの設定を含む)

private:
	// フォント: 言語ごとに ImFont を (必要になったときに) 作る。フォントファイルは 1 度だけ読み込んで共有する。
	//   0 = Latin / キリル (Segoe UI), 1 = 日本語, 2 = 簡体字中国語, 3 = 韓国語。どれも Windows 標準フォント。
	void LoadFontFiles();
	void ApplyLanguageFont();
	std::array<std::vector<std::uint8_t>, 4> m_fontData;
	std::array<ImFont*, static_cast<size_t>( i18n::Lang::Count )> m_langFonts{};
	float m_dpiScale = 1.0f;

	bool m_ready = false;
	SrvHeap* m_srv = nullptr;
	std::string m_iniPath; // ImGui の配置保存先 (io.IniFilename は文字列を保持しないので寿命をここで管理)
};
