#include "ui/UiContext.h"

#include <imgui.h>

#include <cstdio>

#include "ui/UiLayout.h"

// レイアウト:  左 = IBL Bake (入力・設定)   中央 = 3D View   右 = Material / Lighting (オブジェクト・マテリアル・表示)
//   ドッキングは ImGui の docking ブランチ。配置は imgui.ini に保存される (View > Reset layout で既定に戻す)。
namespace ui
{
	void BuildMainUi( UiContext& ctx )
	{
		AppModel& m = ctx.model;
		SceneParams& scene = m.scene;

		if( !m.view.showUi )
		{
			// UI 非表示: ウィンドウ全体を 3D ビューにする
			scene.viewX = scene.viewY = 0.0f;
			scene.viewW = static_cast<float>( ctx.renderer.Width() );
			scene.viewH = static_cast<float>( ctx.renderer.Height() );
			m.viewMouse.hovered = true;
			m.viewMouse.active = ImGui::IsMouseDown( ImGuiMouseButton_Left );
			return;
		}

		const bool resetLayout = DrawMenuBar( ctx );
		if( resetLayout ) m.view.showBakePanel = m.view.showMaterialPanel = true; // 閉じていたパネルも戻す

		DockHostBegin( ctx.dpiScale, resetLayout ); // メニューバーの後 (作業領域から差し引かれるため)
		DockHostEnd();

		if( m.view.showBakePanel )
		{
			if( ImGui::Begin( kPanelBake, &m.view.showBakePanel ) ) DrawBakePanel( ctx );
			ImGui::End();
		}

		char overlay[128];
		if( ctx.renderer.IsIBLReady() )
		{
			if( m.view.alwaysRender ) std::snprintf( overlay, sizeof( overlay ), "%.1f FPS    Drag: orbit   Wheel: zoom   TAB: hide UI", ImGui::GetIO().Framerate );
			else                      std::snprintf( overlay, sizeof( overlay ), "Drag: orbit   Wheel: zoom   TAB: hide UI" );
		}
		else
			std::snprintf( overlay, sizeof( overlay ), "File > Open... to load an HDRI, then press Bake in \"%s\".", kPanelBake );
		const ViewInfo v = DrawViewPanel( ctx.dpiScale, overlay );
		scene.viewX = v.x;
		scene.viewY = v.y;
		scene.viewW = v.w;
		scene.viewH = v.h;
		m.viewMouse.hovered = v.hovered;
		m.viewMouse.active = v.active;

		if( m.view.showMaterialPanel )
		{
			if( ImGui::Begin( kPanelMaterial, &m.view.showMaterialPanel ) ) DrawMaterialPanel( ctx );
			ImGui::End();
		}
	}
} // namespace ui
