#include "ui/UiContext.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace ui
{
	namespace
	{
		void DrawObjectSection( UiContext& ctx )
		{
			SceneParams& scene = ctx.model.scene;
			const Renderer& renderer = ctx.renderer;
			ImGui::SeparatorText( "Object" );
			scene.object = std::clamp( scene.object, 0, static_cast<int>( SceneObject_Count ) - 1 );
			if( ImGui::BeginCombo( "Shape", SceneObjectName( scene.object ) ) )
			{
				for( int i = 0; i < SceneObject_Count; ++i )
				{
					ImGui::BeginDisabled( !renderer.IsObjectAvailable( i ) );
					if( ImGui::Selectable( SceneObjectName( i ), scene.object == i ) ) scene.object = i;
					ImGui::EndDisabled();
				}
				ImGui::EndCombo();
			}
			if( !renderer.IsObjectAvailable( scene.object ) )
			{
				// .mesh が読めなかった場合は描画側で球にフォールバックしている
				ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.6f, 0.3f, 1.0f ) );
				ImGui::TextWrapped( "Model unavailable (showing sphere): %s", renderer.ObjectError( scene.object ).c_str() );
				ImGui::PopStyleColor();
			}
		}

		void DrawMaterialSection( SceneParams& scene )
		{
			ImGui::SeparatorText( "Material" );
			ImGui::SliderFloat( "Roughness", &scene.roughness, 0.0f, 1.0f );
			ImGui::SliderFloat( "Metallic", &scene.metallic, 0.0f, 1.0f );
			ImGui::SliderFloat( "Reflectance", &scene.reflectance, 0.0f, 1.0f );
			ImGui::ColorEdit3( "Albedo", &scene.albedo.x );
		}

		void DrawIblSection( AppModel& m )
		{
			SceneParams& scene = m.scene;
			ImGui::SeparatorText( "IBL" );
			ImGui::Checkbox( "Diffuse (Irradiance Cube)", &scene.useDiffuse );
			ImGui::Checkbox( "Specular (Prefiltered + LUT)", &scene.useSpecular );
			ImGui::SliderFloat( "Diffuse intensity", &scene.diffuseIntensity, 0.0f, 4.0f );
			ImGui::SliderFloat( "Specular intensity", &scene.specularIntensity, 0.0f, 4.0f );
			ImGui::Checkbox( "Multi-scatter compensation", &scene.multiScatter );
			ImGui::SliderFloat( "Env rotation (deg)", &m.view.envRotationDeg, 0.0f, 360.0f );
		}

		// アンチエイリアス (TAA): 3D ビューだけが対象 (UI には掛からない)。シェーダー内のスペキュラのちらつきも時間方向に平均される。
		// 静止すると履歴が収束して最もきれいになり、カメラを動かしている間は少しぼける / 輪郭が揺れることがある。
		void DrawAntiAliasingControls( ViewOptions& v )
		{
			ImGui::Checkbox( "Anti-aliasing (TAA)", &v.taaEnabled );
			if( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "Temporal anti-aliasing: jitters the camera every frame and accumulates the history.\nThe image converges after a moment when nothing moves." );
		}

		void DrawDisplaySection( UiContext& ctx )
		{
			AppModel& m = ctx.model;
			SceneParams& scene = m.scene;
			ImGui::SeparatorText( "Display" );
			ImGui::SliderFloat( "Exposure (EV)", &scene.exposureEV, -6.0f, 6.0f );
			static const char* tonemaps[] = { "ACES", "Reinhard", "Clamp (none)" };
			ImGui::Combo( "Tonemap", &scene.tonemap, tonemaps, 3 );
			ImGui::Checkbox( "Show sky", &scene.showSky );
			ImGui::SliderFloat( "Sky blur (mip)", &scene.skyBlur, 0.0f, 1.0f );
			ImGui::Checkbox( "VSync", &m.view.vsync );
			DrawAntiAliasingControls( m.view );
			ImGui::Checkbox( "Continuous rendering", &m.view.alwaysRender );
			if( ImGui::IsItemHovered() ) ImGui::SetTooltip( "Off (default): redraw only when there is input or a bake is running.\nOn: redraw every frame (shows FPS in the 3D view)." );
			if( ImGui::Button( "Reset camera" ) ) ctx.camera.Reset();
		}
	} // namespace

	// 右パネル: オブジェクト・マテリアル・IBL・表示
	void DrawMaterialPanel( UiContext& ctx )
	{
		const float s = ctx.dpiScale;
		ImGui::PushItemWidth( -150.0f * s );
		DrawObjectSection( ctx );
		DrawMaterialSection( ctx.model.scene );
		DrawIblSection( ctx.model );
		DrawDisplaySection( ctx );

		if( ctx.renderer.IsIBLReady() && ImGui::CollapsingHeader( "BRDF LUT preview (R=scale, G=bias)" ) )
		{
			const ImTextureID tex = (ImTextureID)(uintptr_t)ctx.renderer.BrdfLutGpuHandle().ptr;
			ImGui::Image( tex, ImVec2( 192 * s, 192 * s ) );
			ImGui::TextDisabled( "x: NdotV  y: roughness (top = 0)" );
		}
		ImGui::Separator();
		ImGui::TextDisabled( "Drag in the 3D view: orbit / Wheel: zoom / TAB: hide UI" );
		ImGui::PopItemWidth();
	}
} // namespace ui
