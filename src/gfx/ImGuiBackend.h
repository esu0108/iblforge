// ImGuiBackend.h : Dear ImGui の初期化 (コンテキスト / フォント / Win32 + DX12 バックエンド) とフレームの開始・描画
#pragma once
#include "gfx/GpuContext.h"
#include "gfx/SrvHeap.h"

#include <string>

class ImGuiBackend
{
public:
	~ImGuiBackend() { Shutdown(); }

	// GPU / SrvHeap が初期化済みであること。DPI スケールでスタイルとフォントを拡大する
	void Init( HWND hwnd, float dpiScale, GpuContext& gpu, SrvHeap& srv, UINT frameCount );
	void Shutdown(); // 呼ぶ前に GPU の完了を待っておくこと

	void NewFrame();                                // ImGui::NewFrame まで行う
	void Render( ID3D12GraphicsCommandList* cmd );  // ImGui::Render + 描画コマンドの記録 (SRV ヒープの設定を含む)

private:
	bool m_ready = false;
	SrvHeap* m_srv = nullptr;
	std::string m_iniPath; // ImGui の配置保存先 (io.IniFilename は文字列を保持しないので寿命をここで管理)
};
