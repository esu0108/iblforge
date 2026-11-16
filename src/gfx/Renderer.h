// Renderer.h : 描画のファサード。GPU / スワップチェーン / シーン / テクスチャ / ImGui の各部品を組み合わせて 1 フレームを回す。
//              それぞれの責務は個別のクラスにあり、Renderer は「部品の生成順・破棄順」と「フレームの流れ」だけを持つ。
//
//   GpuContext      デバイス / キュー / フェンス
//   SrvHeap         シェーダーから見える SRV ヒープ (シーン用スロット + ImGui 用の動的スロット)
//   RenderTargets   スワップチェーン / バックバッファ / 深度
//   SceneTargets    シーンのオフスクリーンターゲット (カラー / 速度) と TAA の履歴
//   FrameCommands   フレームごとのコマンドアロケーターと完了フェンス
//   SceneRenderer   ルートシグネチャ / PSO / 定数バッファ。シーンの描画 (ジッタ付き)
//   TaaResolver     TAA のジッタ列と解決パス (シーン + 履歴 → バックバッファ)
//   MeshLibrary     球 + .mesh モデルの GPU バッファ
//   IblTextures     Prefiltered / LUT / Irradiance テクスチャ
//   PreviewTexture  HDRI プレビュー
//   ImGuiBackend    ImGui の初期化 / フレーム / 描画
#pragma once
#include "gfx/FrameCommands.h"
#include "gfx/GpuContext.h"
#include "gfx/IblTextures.h"
#include "gfx/ImGuiBackend.h"
#include "gfx/MeshLibrary.h"
#include "gfx/PreviewTexture.h"
#include "gfx/RenderTargets.h"
#include "gfx/SceneParams.h"
#include "gfx/SceneRenderer.h"
#include "gfx/SrvHeap.h"
#include "gfx/TaaResolver.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace ibl { struct BakedTextures; }

class Renderer
{
public:
	Renderer() = default;
	~Renderer() { Shutdown(); }
	Renderer( const Renderer& ) = delete;
	Renderer& operator=( const Renderer& ) = delete;

	void Init( HWND hwnd, UINT width, UINT height );          // 例外を投げる
	void InitImGui( HWND hwnd, float dpiScale );              // Init の後に呼ぶ
	void Shutdown();
	void Resize( UINT width, UINT height );
	void WaitForGpu() { m_gpu.WaitIdle(); }
	ID3D12Device* Device() const { return m_gpu.Device(); } // GPU ベイクと共有する (D3D12 デバイスはスレッドセーフ)

	// ---- IBL / プレビュー (フレーム外で呼ぶこと)
	bool LoadIBL( const std::filesystem::path& prefilteredDds, const std::filesystem::path& lutDds,
				  const std::filesystem::path& irradianceDds, std::string& err ) { return m_ibl.LoadFiles( prefilteredDds, lutDds, irradianceDds, err ); }
	bool LoadIBL( const ibl::BakedTextures& baked, std::string& err ) { return m_ibl.LoadBaked( baked, err ); }
	bool IsIBLReady() const { return m_ibl.Ready(); }
	D3D12_GPU_DESCRIPTOR_HANDLE BrdfLutGpuHandle() const { return m_ibl.BrdfLutGpu(); } // ImGui::Image 用

	bool SetPreviewImage( const uint8_t* rgba, UINT width, UINT height, std::string& err ) { return m_preview.Set( rgba, width, height, err ); }
	bool HasPreview() const { return m_preview.Has(); }
	D3D12_GPU_DESCRIPTOR_HANDLE PreviewGpuHandle() const { return m_preview.Gpu(); }

	// ---- アンチエイリアス (TAA)。シーン (3D ビュー) だけが対象で、UI には掛からない。切り替えは次のフレームから反映される。
	void SetTaaEnabled( bool enabled );
	bool TaaEnabled() const { return m_taaEnabled; }

	// ---- オブジェクト (.mesh が見つからない / 壊れている場合は選べない)
	bool IsObjectAvailable( int object ) const { return m_meshes.IsAvailable( object ); }
	const std::string& ObjectError( int object ) const { return m_meshes.Error( object ); }

	// ---- フレーム
	void BeginFrame();                       // ImGui::NewFrame まで行う
	void DrawScene( const SceneParams& p );
	void EndFrame( bool vsync );             // ImGui 描画 → Present

	UINT Width() const { return m_targets.Width(); }
	UINT Height() const { return m_targets.Height(); }

private:
	// 宣言順 = 生成順 (破棄は逆順)。Shutdown() で GPU の完了を待ってから明示的に解放する。
	GpuContext     m_gpu;
	SrvHeap        m_srv;
	RenderTargets  m_targets;
	SceneTargets   m_sceneTargets;
	FrameCommands  m_frames;
	SceneRenderer  m_scene;
	TaaResolver    m_taa;
	MeshLibrary    m_meshes;
	IblTextures    m_ibl;
	PreviewTexture m_preview;
	ImGuiBackend   m_imgui;

	ID3D12GraphicsCommandList* m_cmd = nullptr; // BeginFrame 〜 EndFrame の間だけ有効
	UINT m_frameIndex = 0;
	bool m_backBufferBegun = false;  // このフレームでバックバッファのパスを開始済みか (シーンを描かなかったフレームは EndFrame で開始する)
	bool m_taaEnabled = true;
};
