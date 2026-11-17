#include "gfx/ImGuiBackend.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <fstream>

namespace
{
	SrvHeap* g_srv = nullptr; // ImGui のコールバック (キャプチャ不可) 用

	void ImGuiSrvAlloc( ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu )
	{
		g_srv->AllocDynamic( cpu, gpu );
	}
	void ImGuiSrvFree( ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE )
	{
		g_srv->FreeDynamic( cpu );
	}
} // namespace

void ImGuiBackend::Init( HWND hwnd, float dpiScale, GpuContext& gpu, SrvHeap& srv, UINT frameCount )
{
	m_srv = &srv;
	g_srv = &srv;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// パネル配置 (ドッキング) を exe の隣の imgui.ini に保存・復元する。無ければ既定の 3 ペイン配置 (UiLayout.cpp) になる。
	m_iniPath = util::PathToUtf8( util::ExeDir() / L"imgui.ini" );
	io.IniFilename = m_iniPath.c_str();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();
	ImGui::GetStyle().ScaleAllSizes( dpiScale );
	m_dpiScale = dpiScale;
	LoadFontFiles();
	ApplyLanguageFont();

	ImGui_ImplWin32_Init( hwnd );
	ImGui_ImplDX12_InitInfo info;
	info.Device = gpu.Device();
	info.CommandQueue = gpu.Queue();
	info.NumFramesInFlight = static_cast<int>( frameCount );
	info.RTVFormat = gpu::kBackBufferFormat;
	info.DSVFormat = gpu::kDepthFormat;
	info.SrvDescriptorHeap = srv.Heap();
	info.SrvDescriptorAllocFn = ImGuiSrvAlloc;
	info.SrvDescriptorFreeFn = ImGuiSrvFree;
	ImGui_ImplDX12_Init( &info );
	m_ready = true;
}

namespace
{
	bool ReadFileBytes( const std::filesystem::path& file, std::vector<std::uint8_t>& out )
	{
		std::ifstream in( file, std::ios::binary | std::ios::ate );
		if( !in ) return false;
		const std::streamsize size = in.tellg();
		if( size <= 0 ) return false;
		out.resize( static_cast<size_t>( size ) );
		in.seekg( 0 );
		return static_cast<bool>( in.read( reinterpret_cast<char*>( out.data() ), size ) );
	}
} // namespace

void ImGuiBackend::LoadFontFiles()
{
	wchar_t winDir[MAX_PATH]{};
	GetWindowsDirectoryW( winDir, MAX_PATH );
	const std::filesystem::path fonts = std::filesystem::path( winDir ) / L"Fonts";
	// 各スロットの候補 (先に見つかったものを使う)。ファイルは常駐するので 1 スロット 1 ファイルにする。
	const std::vector<const wchar_t*> candidates[4] = {
		{ L"segoeui.ttf" },
		{ L"meiryo.ttc", L"YuGothR.ttc", L"msgothic.ttc" },
		{ L"msyh.ttc", L"msyh.ttf" },
		{ L"malgun.ttf" },
	};
	for( size_t i = 0; i < m_fontData.size(); ++i )
		for( const wchar_t* name : candidates[i] )
			if( ReadFileBytes( fonts / name, m_fontData[i] ) ) break;
}

void ImGuiBackend::ApplyLanguageFont()
{
	const i18n::Lang lang = i18n::Current();
	ImGuiIO& io = ImGui::GetIO();
	ImFont*& font = m_langFonts[static_cast<size_t>( lang )];
	if( !font )
	{
		// 先頭が主フォント。漢字は先に並べたフォントの字形が使われるので、言語ごとに順序を変える (日本語 → 日本語フォントの字形)。
		// 残りはフォールバックとして合成する (ファイル名やメニューの言語名に他言語の文字が出ても表示できる)。
		int order[4] = { 0, 1, 2, 3 };
		switch( lang )
		{
		case i18n::Lang::Ja:   { const int o[4] = { 1, 0, 2, 3 }; std::copy( o, o + 4, order ); break; }
		case i18n::Lang::ZhCN: { const int o[4] = { 2, 0, 1, 3 }; std::copy( o, o + 4, order ); break; }
		case i18n::Lang::Ko:   { const int o[4] = { 3, 0, 1, 2 }; std::copy( o, o + 4, order ); break; }
		default: break;
		}
		for( int slot : order )
		{
			std::vector<std::uint8_t>& data = m_fontData[static_cast<size_t>( slot )];
			if( data.empty() ) continue;
			ImFontConfig cfg;
			cfg.FontDataOwnedByAtlas = false; // データは m_fontData が持つ (言語間で共有)
			cfg.MergeMode = ( font != nullptr );
			ImFont* f = io.Fonts->AddFontFromMemoryTTF( data.data(), static_cast<int>( data.size() ), 16.0f * m_dpiScale, &cfg );
			if( f && !font ) font = f;
		}
		if( !font )
		{
			ImFontConfig cfg;
			cfg.SizePixels = 13.0f * m_dpiScale;
			font = io.Fonts->AddFontDefault( &cfg );
		}
	}
	io.FontDefault = font;
}

void ImGuiBackend::Shutdown()
{
	if( !m_ready ) return;
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_ready = false;
	g_srv = nullptr;
}

void ImGuiBackend::NewFrame()
{
	ApplyLanguageFont(); // 言語が切り替わっていれば、フレームの開始前にフォントを差し替える
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiBackend::Render( ID3D12GraphicsCommandList* cmd )
{
	ImGui::Render();
	ID3D12DescriptorHeap* heaps[] = { m_srv->Heap() };
	cmd->SetDescriptorHeaps( 1, heaps );
	ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), cmd );
}
