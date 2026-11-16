#include "ui/UiContext.h"

#include <imgui.h>

#include "ui/UiLayout.h"

namespace ui
{
	bool DrawMenuBar( UiContext& ctx )
	{
		AppModel& m = ctx.model;
		bool resetLayout = false;
		if( !ImGui::BeginMainMenuBar() ) return false;

		if( ImGui::BeginMenu( "File" ) )
		{
			const bool running = ctx.workflow.IsBaking();
			if( ImGui::MenuItem( "Open HDRI...", "Ctrl+O", false, !ctx.workflow.IsLoading() ) ) m.requests.open = true;
			ImGui::Separator();
			if( ImGui::MenuItem( "Import baked textures...", "Ctrl+I", false, !running ) ) m.requests.import = true;
			if( ImGui::MenuItem( "Save baked textures...", "Ctrl+S", false, m.results.hasBaked && !running ) ) m.requests.save = true;
			ImGui::Separator();
			if( ImGui::MenuItem( "Exit", "Alt+F4" ) ) PostMessageW( ctx.hwnd, WM_CLOSE, 0, 0 );
			ImGui::EndMenu();
		}
		if( ImGui::BeginMenu( "View" ) )
		{
			ImGui::MenuItem( kPanelBake, nullptr, &m.view.showBakePanel );
			ImGui::MenuItem( kPanelMaterial, nullptr, &m.view.showMaterialPanel );
			ImGui::Separator();
			if( ImGui::MenuItem( "Reset layout" ) ) resetLayout = true;
			if( ImGui::MenuItem( "Hide UI", "TAB" ) ) m.view.showUi = false;
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
		return resetLayout;
	}
} // namespace ui
