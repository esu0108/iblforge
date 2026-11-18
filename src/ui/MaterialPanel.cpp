#include "ui/UiContext.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

#include "i18n/I18n.h"
#include "ui/UiText.h"

namespace ui
{
	using i18n::Str;
	using i18n::T;

	namespace
	{
		// 固有名詞のモデル名 (Bunny など) はそのまま、球体だけ翻訳する
		const char* ObjectLabel( int object ) { return object == SceneObject_Sphere ? T( Str::Sphere ) : SceneObjectName( object ); }

		void DrawObjectSection( UiContext& ctx )
		{
			SceneParams& scene = ctx.model.scene;
			const Renderer& renderer = ctx.renderer;
			ImGui::SeparatorText( T( Str::SecObject ) );
			scene.object = std::clamp( scene.object, 0, static_cast<int>( SceneObject_Count ) - 1 );
			if( ImGui::BeginCombo( T( Str::Shape ), ObjectLabel( scene.object ) ) )
			{
				for( int i = 0; i < SceneObject_Count; ++i )
				{
					ImGui::BeginDisabled( !renderer.IsObjectAvailable( i ) );
					if( ImGui::Selectable( ObjectLabel( i ), scene.object == i ) ) scene.object = i;
					ImGui::EndDisabled();
				}
				ImGui::EndCombo();
			}
			if( !renderer.IsObjectAvailable( scene.object ) )
			{
				// .mesh が読めなかった場合は描画側で球にフォールバックしている
				ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.6f, 0.3f, 1.0f ) );
				ImGui::TextWrapped( T( Str::ModelUnavailable ), renderer.ObjectError( scene.object ).c_str() );
				ImGui::PopStyleColor();
			}
		}

		void DrawMaterialSection( SceneParams& scene )
		{
			ImGui::SeparatorText( T( Str::SecMaterial ) );
			ImGui::SliderFloat( T( Str::Roughness ), &scene.roughness, 0.0f, 1.0f );
			ImGui::SliderFloat( T( Str::Metallic ), &scene.metallic, 0.0f, 1.0f );
			ImGui::SliderFloat( T( Str::Reflectance ), &scene.reflectance, 0.0f, 1.0f );
			ImGui::ColorEdit3( T( Str::Albedo ), &scene.albedo.x );
		}

		void DrawIblSection( AppModel& m )
		{
			SceneParams& scene = m.scene;
			ImGui::SeparatorText( "IBL" );
			ImGui::Checkbox( T( Str::DiffuseOn ), &scene.useDiffuse );
			// 拡散光の入力: Irradiance Cube か SH (L2)。SH は係数があるとき (ベイク後 / *_sh.json 付きのインポート) だけ選べる
			{
				const char* const names[] = { T( Str::DiffuseSrcCube ), T( Str::DiffuseSrcSh ) };
				scene.diffuseSource = std::clamp( scene.diffuseSource, 0, static_cast<int>( DiffuseSource_Count ) - 1 );
				const bool shActive = scene.diffuseSource == DiffuseSource_SH && !scene.shAvailable; // SH を選んだまま係数が無くなった: 描画は Cube にフォールバック
				if( ImGui::BeginCombo( T( Str::DiffuseSource ), names[shActive ? DiffuseSource_IrradianceCube : scene.diffuseSource] ) )
				{
					for( int i = 0; i < DiffuseSource_Count; ++i )
					{
						ImGui::BeginDisabled( i == DiffuseSource_SH && !scene.shAvailable );
						if( ImGui::Selectable( names[i], scene.diffuseSource == i ) ) scene.diffuseSource = i;
						ImGui::EndDisabled();
					}
					ImGui::EndCombo();
				}
			}
			ImGui::Checkbox( T( Str::SpecularOn ), &scene.useSpecular );
			ImGui::SliderFloat( T( Str::DiffuseIntensity ), &scene.diffuseIntensity, 0.0f, 4.0f );
			ImGui::SliderFloat( T( Str::SpecularIntensity ), &scene.specularIntensity, 0.0f, 4.0f );
			ImGui::Checkbox( T( Str::MultiScatter ), &scene.multiScatter );
			ImGui::SliderFloat( T( Str::EnvRotation ), &m.view.envRotationDeg, 0.0f, 360.0f );
		}

		// アンチエイリアス (TAA): 3D ビューだけが対象 (UI には掛からない)。シェーダー内のスペキュラのちらつきも時間方向に平均される。
		// 静止すると履歴が収束して最もきれいになり、カメラを動かしている間は少しぼける / 輪郭が揺れることがある。
		void DrawAntiAliasingControls( ViewOptions& v )
		{
			ImGui::Checkbox( T( Str::Taa ), &v.taaEnabled );
			if( ImGui::IsItemHovered() )
				ImGui::SetTooltip( "%s", T( Str::TaaTip ) );
		}

		void DrawDisplaySection( UiContext& ctx )
		{
			AppModel& m = ctx.model;
			SceneParams& scene = m.scene;
			ImGui::SeparatorText( T( Str::SecDisplay ) );
			ImGui::SliderFloat( T( Str::Exposure ), &scene.exposureEV, -6.0f, 6.0f );
			const char* tonemaps[] = { "ACES", "Reinhard", T( Str::TonemapClamp ) };
			ImGui::Combo( T( Str::Tonemap ), &scene.tonemap, tonemaps, 3 );
			ImGui::Checkbox( T( Str::ShowSky ), &scene.showSky );
			ImGui::SliderFloat( T( Str::SkyBlur ), &scene.skyBlur, 0.0f, 1.0f );
			ImGui::Checkbox( T( Str::Vsync ), &m.view.vsync );
			DrawAntiAliasingControls( m.view );
			ImGui::Checkbox( T( Str::Continuous ), &m.view.alwaysRender );
			if( ImGui::IsItemHovered() ) ImGui::SetTooltip( "%s", T( Str::ContinuousTip ) );
			if( ImGui::Button( T( Str::ResetCamera ) ) ) ctx.camera.Reset();
		}
	} // namespace

	// 右パネル: オブジェクト・マテリアル・IBL・表示
	void DrawMaterialPanel( UiContext& ctx )
	{
		const float s = ctx.dpiScale;
		ImGui::PushItemWidth( -LabelColumnWidth( { T( Str::Shape ), T( Str::Roughness ), T( Str::Metallic ), T( Str::Reflectance ), T( Str::Albedo ),
			T( Str::DiffuseSource ), T( Str::DiffuseIntensity ), T( Str::SpecularIntensity ), T( Str::EnvRotation ), T( Str::Exposure ), T( Str::Tonemap ), T( Str::SkyBlur ) }, s ) );
		DrawObjectSection( ctx );
		DrawMaterialSection( ctx.model.scene );
		DrawIblSection( ctx.model );
		DrawDisplaySection( ctx );

		if( ctx.renderer.IsIBLReady() && ImGui::CollapsingHeader( T( Str::BrdfPreview ) ) )
		{
			const ImTextureID tex = (ImTextureID)(uintptr_t)ctx.renderer.BrdfLutGpuHandle().ptr;
			ImGui::Image( tex, ImVec2( 192 * s, 192 * s ) );
			TextDisabledWrapped( T( Str::BrdfAxes ) );
		}
		ImGui::Separator();
		TextDisabledWrapped( T( Str::OverlayHelp ) );
		ImGui::PopItemWidth();
	}
} // namespace ui
