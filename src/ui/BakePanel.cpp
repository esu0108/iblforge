#include "ui/UiContext.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>

#include "ui/UiLayout.h"

namespace ui
{
	namespace
	{
		bool ComboSize( const char* label, uint32_t& value, const uint32_t* items, int count )
		{
			int cur = 0;
			for( int i = 0; i < count; ++i ) if( items[i] == value ) cur = i;
			char preview[16];
			std::snprintf( preview, sizeof( preview ), "%u", items[cur] );
			bool changed = false;
			if( ImGui::BeginCombo( label, preview ) )
			{
				for( int i = 0; i < count; ++i )
				{
					char buf[16];
					std::snprintf( buf, sizeof( buf ), "%u", items[i] );
					const bool sel = ( i == cur );
					if( ImGui::Selectable( buf, sel ) ) { value = items[i]; changed = true; }
					if( sel ) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		// 読み込んだ HDRI の表示 (等距円筒図法。トーンマップは表示用だけで、ベイクには影響しない)
		void DrawHdrSection( UiContext& ctx )
		{
			AppModel& m = ctx.model;
			HdrDocument& hdr = m.hdr;
			const ImGuiStyle& style = ImGui::GetStyle();
			const bool loading = ctx.workflow.IsLoading();

			ImGui::SeparatorText( "HDRI" );
			if( hdr.Loaded() ) ImGui::TextWrapped( "%s  (%u x %u)", util::PathToUtf8( hdr.path.filename() ).c_str(), hdr.image->width, hdr.image->height );
			else if( !loading ) ImGui::TextDisabled( "No HDRI loaded. Use File > Open... or drop a .hdr file." );
			if( loading )
			{
				// 読み込みの進捗は取得できない (DirectXTex にコールバックが無い) ので、動き続けるバーで「処理中」を示す。
				// 負の fraction は ImGui の不定 (往復アニメーション) 表示。メインループは読み込み中 50ms ごとに描画するので動き続ける。
				ImGui::ProgressBar( -1.0f * static_cast<float>( ImGui::GetTime() ), ImVec2( -FLT_MIN, 0.0f ), "Loading HDRI..." );
			}

			const float imgW = std::max( ImGui::GetContentRegionAvail().x, 16.0f );
			if( hdr.Loaded() && ctx.renderer.HasPreview() && hdr.preview.w > 0 )
			{
				const ImTextureID tex = (ImTextureID)(uintptr_t)ctx.renderer.PreviewGpuHandle().ptr;
				ImGui::Image( tex, ImVec2( imgW, imgW * static_cast<float>( hdr.preview.h ) / static_cast<float>( hdr.preview.w ) ) );
				ImGui::SetNextItemWidth( -( ImGui::CalcTextSize( "Auto" ).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x ) );
				if( ImGui::SliderFloat( "##previewEV", &hdr.previewEV, -10.0f, 10.0f, "Preview exposure %.1f EV" ) ) hdr.previewDirty = true;
				ImGui::SameLine();
				if( ImGui::Button( "Auto" ) ) { hdr.previewEV = hdr.preview.autoEV; hdr.previewDirty = true; }
			}
			else
			{
				// 未読み込み: 枠だけ表示 (2:1 = 等距円筒図法の比率)
				const ImVec2 p0 = ImGui::GetCursorScreenPos();
				const ImVec2 sz( imgW, imgW * 0.5f );
				ImGui::Dummy( sz );
				ImGui::GetWindowDrawList()->AddRect( p0, ImVec2( p0.x + sz.x, p0.y + sz.y ), ImGui::GetColorU32( ImGuiCol_Border ) );
			}
		}

		// ベイク設定と Bake ボタン
		void DrawSettingsSection( UiContext& ctx, bool running )
		{
			ibl::BakeSettings& s = ctx.model.settings;
			ImGui::SeparatorText( "Bake settings" );
			ImGui::BeginDisabled( running );
			{
				int device = s.useGpu ? 1 : 0;
				ImGui::TextUnformatted( "Processor" );
				ImGui::SameLine();
				ImGui::RadioButton( "CPU", &device, 0 );
				ImGui::SameLine();
				ImGui::RadioButton( "GPU (D3D12 compute)", &device, 1 );
				s.useGpu = ( device == 1 );
				if( s.useGpu ) ImGui::TextDisabled( "Runs as D3D12 compute shaders; output files are identical in format." );
			}
			static const uint32_t srcSizes[] = { 128, 256, 512, 1024, 2048 };
			static const uint32_t pfSizes[]  = { 64, 128, 256, 512, 1024 };
			static const uint32_t lutSizes[] = { 64, 128, 256, 512 };
			static const uint32_t irrSizes[] = { 16, 32, 64, 128 };
			{
				static const ibl::CubeFormat formats[] = { ibl::CubeFormat::Float16, ibl::CubeFormat::Float32, ibl::CubeFormat::R11G11B10, ibl::CubeFormat::BC6H };
				static const char* const hints[] = {
					"RGBA16F (default)", "RGBA32F (lossless, 2x size)", "R11G11B10F (1/2 size, lower precision)", "BC6H (compressed, lossy, slow to encode)" };
				int cur = 0;
				for( int i = 0; i < 4; ++i ) if( formats[i] == s.cubeFormat ) cur = i;
				if( ImGui::BeginCombo( "Cubemap format", hints[cur] ) )
				{
					for( int i = 0; i < 4; ++i )
						if( ImGui::Selectable( hints[i], i == cur ) ) s.cubeFormat = formats[i];
					ImGui::EndCombo();
				}
			}
			ComboSize( "Source cube size", s.sourceCubeSize, srcSizes, 5 );
			ComboSize( "Prefilter size", s.prefilterSize, pfSizes, 5 );
			int ps = static_cast<int>( s.prefilterSamples );
			if( ImGui::InputInt( "Prefilter samples", &ps, 128, 1024 ) ) s.prefilterSamples = static_cast<uint32_t>( std::clamp( ps, 16, 16384 ) );
			ComboSize( "Irradiance size", s.irradianceSize, irrSizes, 4 );
			int is = static_cast<int>( s.irradianceSamples );
			if( ImGui::InputInt( "Irradiance samples", &is, 256, 1024 ) ) s.irradianceSamples = static_cast<uint32_t>( std::clamp( is, 16, 16384 ) );
			ComboSize( "BRDF LUT size", s.lutSize, lutSizes, 4 );
			int ls = static_cast<int>( s.lutSamples );
			if( ImGui::InputInt( "BRDF LUT samples", &ls, 128, 1024 ) ) s.lutSamples = static_cast<uint32_t>( std::clamp( ls, 16, 16384 ) );
			ImGui::InputFloat( "Sample clamp (0 = off)", &s.maxSampleValue, 10.0f, 100.0f, "%.1f" );
			s.maxSampleValue = std::max( 0.0f, s.maxSampleValue );
			ImGui::BeginDisabled( !ctx.model.hdr.Loaded() );
			if( ImGui::Button( "Bake" ) ) ctx.workflow.StartBake();
			ImGui::EndDisabled();
			ImGui::EndDisabled(); // running
		}

		// 保存状態 / 進捗 / ステータス / ファイル一覧
		void DrawResultSection( UiContext& ctx, bool running )
		{
			const AppModel& m = ctx.model;
			const BakeResults& r = m.results;

			// ベイク結果は File > Save... で初めてファイルになる
			ImGui::SameLine();
			if( !r.hasBaked )        ImGui::TextDisabled( "Nothing to save yet." );
			else if( r.bakedSaved )  ImGui::TextDisabled( "Saved." );
			else                     ImGui::TextColored( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ), "Not saved yet (File > Save...)" );

			if( running )
			{
				const BakeController& bake = ctx.workflow.Bake();
				char overlay[96];
				std::snprintf( overlay, sizeof( overlay ), "%s  %d%%", bake.Stage(), static_cast<int>( bake.Progress() * 100.0f ) );
				const ImGuiStyle& style = ImGui::GetStyle();
				const float cancelW = ImGui::CalcTextSize( "Cancel" ).x + style.FramePadding.x * 2.0f;
				ImGui::ProgressBar( bake.Progress(), ImVec2( -( cancelW + style.ItemSpacing.x ), 0 ), overlay );
				ImGui::SameLine();
				ImGui::BeginDisabled( ctx.workflow.IsCancelling() );
				if( ImGui::Button( "Cancel" ) ) ctx.workflow.CancelBake();
				ImGui::EndDisabled();
			}
			ImGui::TextWrapped( "%s", m.status.c_str() );

			if( r.hasOutputs )
			{
				ImGui::SeparatorText( !r.hasBaked || r.bakedSaved ? "Files" : "Files (previous save / import)" );
				ImGui::TextWrapped( "%s", util::PathToUtf8( r.outputs.prefilteredCube ).c_str() );
				ImGui::TextWrapped( "%s", util::PathToUtf8( r.outputs.brdfLut ).c_str() );
				ImGui::TextWrapped( "%s", util::PathToUtf8( r.outputs.irradianceCube ).c_str() );
			}
		}
	} // namespace

	// 左パネル: ファイル読み込み・ベイク設定
	void DrawBakePanel( UiContext& ctx )
	{
		const bool running = ctx.workflow.IsBaking();
		DrawHdrSection( ctx );
		ImGui::PushItemWidth( -170.0f * ctx.dpiScale ); // 以降の項目: 右にラベル用の余白を確保
		DrawSettingsSection( ctx, running );
		DrawResultSection( ctx, running );
		ImGui::PopItemWidth();
	}
} // namespace ui
