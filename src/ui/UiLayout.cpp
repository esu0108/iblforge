#include "UiLayout.h"

#include <algorithm>

#include <imgui_internal.h> // DockBuilder (まだ公開 API ではないが docking の既定配置に必要)

namespace ui
{
	void BuildDefaultLayout( ImGuiID dockspaceId, ImVec2 size, float dpiScale )
	{
		ImGui::DockBuilderRemoveNode( dockspaceId ); // 既存の配置を破棄 (Reset layout 用)
		ImGui::DockBuilderAddNode( dockspaceId, ImGuiDockNodeFlags_DockSpace );
		ImGui::DockBuilderSetNodeSize( dockspaceId, size );

		// 左右のパネル幅は「ピクセル」で決め (DPI に追従)、極端に狭い/広いウィンドウでは割合で制限する
		const float total = std::max( size.x, 1.0f );
		const float leftRatio = std::clamp( 400.0f * dpiScale / total, 0.15f, 0.40f );
		const float restW = total * ( 1.0f - leftRatio );
		const float rightRatio = std::clamp( 360.0f * dpiScale / restW, 0.15f, 0.45f );

		ImGuiID left = 0, rest = 0, right = 0, center = 0;
		ImGui::DockBuilderSplitNode( dockspaceId, ImGuiDir_Left, leftRatio, &left, &rest );
		ImGui::DockBuilderSplitNode( rest, ImGuiDir_Right, rightRatio, &right, &center );

		ImGui::DockBuilderDockWindow( kPanelBake, left );
		ImGui::DockBuilderDockWindow( kPanelView, center );
		ImGui::DockBuilderDockWindow( kPanelMaterial, right );
		ImGui::DockBuilderFinish( dockspaceId );
	}

	ImGuiID DockHostBegin( float dpiScale, bool resetLayout )
	{
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos( vp->WorkPos );
		ImGui::SetNextWindowSize( vp->WorkSize );
		ImGui::SetNextWindowViewport( vp->ID );

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
									   ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
									   ImGuiWindowFlags_NoBackground; // 背景は塗らない (シーンを見せる)
		ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
		ImGui::Begin( "##DockHost", nullptr, flags );
		ImGui::PopStyleVar( 3 );

		const ImGuiID id = ImGui::GetID( "MainDockSpace" );
		if( resetLayout || ImGui::DockBuilderGetNode( id ) == nullptr ) BuildDefaultLayout( id, vp->WorkSize, dpiScale );
		ImGui::DockSpace( id, ImVec2( 0.0f, 0.0f ), ImGuiDockNodeFlags_None );
		return id;
	}

	void DockHostEnd()
	{
		ImGui::End();
	}

	ViewInfo DrawViewPanel( float dpiScale, const char* overlayText )
	{
		// 中央ペインはタブを出さない (1 枚きりの 3D ビューなのでタイトルは不要)。他のパネルをここへ重ねてドックするのは可能。
		ImGuiWindowClass cls;
		cls.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
		ImGui::SetNextWindowClass( &cls );

		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.0f, 0.0f ) );
		ImGui::Begin( kPanelView, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground );
		ImGui::PopStyleVar();

		ViewInfo v;
		const ImVec2 pos = ImGui::GetCursorScreenPos();
		const ImVec2 size = ImGui::GetContentRegionAvail();
		v.x = pos.x;
		v.y = pos.y;
		v.w = std::max( size.x, 0.0f );
		v.h = std::max( size.y, 0.0f );
		if( size.x >= 1.0f && size.y >= 1.0f )
		{
			// 矩形全体を 1 つの見えないボタンにして、ドラッグ/ホイールの対象にする (ウィンドウ移動などと干渉させない)
			ImGui::InvisibleButton( "##view", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle );
			v.hovered = ImGui::IsItemHovered();
			v.active = ImGui::IsItemActive();
			if( overlayText && overlayText[0] )
			{
				ImGui::GetWindowDrawList()->AddText( ImVec2( pos.x + 8.0f * dpiScale, pos.y + 6.0f * dpiScale ),
													 ImGui::GetColorU32( ImGuiCol_Text, 0.55f ), overlayText );
			}
		}
		ImGui::End();
		return v;
	}
} // namespace ui
