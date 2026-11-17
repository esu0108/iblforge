#include "ui/UiContext.h"

#include <imgui.h>

#include "i18n/I18n.h"
#include "ui/UiLayout.h"

namespace ui
{
	bool DrawMenuBar( UiContext& ctx )
	{
		AppModel& m = ctx.model;
		using i18n::Str;
		using i18n::T;
		bool resetLayout = false;
		if( !ImGui::BeginMainMenuBar() ) return false;

		if( ImGui::BeginMenu( T( Str::MenuFile ) ) )
		{
			const bool running = ctx.workflow.IsBaking();
			if( ImGui::MenuItem( T( Str::OpenHdri ), "Ctrl+O", false, !ctx.workflow.IsLoading() ) ) m.requests.open = true;
			ImGui::Separator();
			if( ImGui::MenuItem( T( Str::ImportBaked ), "Ctrl+I", false, !running ) ) m.requests.import = true;
			if( ImGui::MenuItem( T( Str::SaveBaked ), "Ctrl+S", false, m.results.hasBaked && !running ) ) m.requests.save = true;
			ImGui::Separator();
			if( ImGui::MenuItem( T( Str::Exit ), "Alt+F4" ) ) PostMessageW( ctx.hwnd, WM_CLOSE, 0, 0 );
			ImGui::EndMenu();
		}
		if( ImGui::BeginMenu( T( Str::MenuView ) ) )
		{
			ImGui::MenuItem( PanelLabel( Panel::Bake ), nullptr, &m.view.showBakePanel );
			ImGui::MenuItem( PanelLabel( Panel::Material ), nullptr, &m.view.showMaterialPanel );
			ImGui::Separator();
			if( ImGui::MenuItem( T( Str::ResetLayout ) ) ) resetLayout = true;
			if( ImGui::MenuItem( T( Str::HideUi ), "TAB" ) ) m.view.showUi = false;
			ImGui::Separator();
			if( ImGui::BeginMenu( T( Str::MenuLanguage ) ) )
			{
				// 言語名はその言語自身の表記 (どの言語で使っていても見つけられるように)
				for( int i = 0; i < static_cast<int>( i18n::Lang::Count ); ++i )
				{
					const i18n::Lang l = static_cast<i18n::Lang>( i );
					if( ImGui::MenuItem( i18n::NativeName( l ), nullptr, l == i18n::Current() ) ) i18n::Set( l );
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
		return resetLayout;
	}
} // namespace ui
