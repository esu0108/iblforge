#include "gfx/ImGuiBackend.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

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

	void LoadFont( ImGuiIO& io, float dpiScale )
	{
		// 日本語パス表示のため、Windows 標準の日本語フォントがあれば使う
		const wchar_t* fonts[] = { L"C:\\Windows\\Fonts\\meiryo.ttc", L"C:\\Windows\\Fonts\\YuGothR.ttc", L"C:\\Windows\\Fonts\\msgothic.ttc" };
		for( const wchar_t* f : fonts )
		{
			if( GetFileAttributesW( f ) == INVALID_FILE_ATTRIBUTES ) continue;
			const std::string path = util::WideToUtf8( f );
			// ImGui 1.92 以降はグリフを必要になったときに読み込む (動的フォント)。glyph_ranges (GetGlyphRangesJapanese 等) は不要で、
			// 指定すると約 3000 字の漢字に制限されてしまうので渡さない。これでパス中の任意の漢字も表示できる。
			if( io.Fonts->AddFontFromFileTTF( path.c_str(), 16.0f * dpiScale ) ) return;
		}
		ImFontConfig cfg;
		cfg.SizePixels = 13.0f * dpiScale;
		io.Fonts->AddFontDefault( &cfg );
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
	LoadFont( io, dpiScale );

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
