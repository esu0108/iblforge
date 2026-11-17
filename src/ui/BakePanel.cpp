#include "ui/UiContext.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>

#include "i18n/I18n.h"
#include "ui/UiLayout.h"
#include "ui/UiText.h"

namespace ui
{
	using i18n::Str;
	using i18n::T;

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
			else if( !loading ) TextDisabledWrapped( T( Str::HdrNone ) );
			if( loading )
			{
				// 読み込みの進捗は取得できない (DirectXTex にコールバックが無い) ので、動き続けるバーで「処理中」を示す。
				// 負の fraction は ImGui の不定 (往復アニメーション) 表示。メインループは読み込み中 50ms ごとに描画するので動き続ける。
				ImGui::ProgressBar( -1.0f * static_cast<float>( ImGui::GetTime() ), ImVec2( -FLT_MIN, 0.0f ), T( Str::LoadingHdri ) );
			}

			const float imgW = std::max( ImGui::GetContentRegionAvail().x, 16.0f );
			if( hdr.Loaded() && ctx.renderer.HasPreview() && hdr.preview.w > 0 )
			{
				const ImTextureID tex = (ImTextureID)(uintptr_t)ctx.renderer.PreviewGpuHandle().ptr;
				ImGui::Image( tex, ImVec2( imgW, imgW * static_cast<float>( hdr.preview.h ) / static_cast<float>( hdr.preview.w ) ) );
				ImGui::SetNextItemWidth( -( ImGui::CalcTextSize( T( Str::Auto ) ).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x ) );
				if( ImGui::SliderFloat( "##previewEV", &hdr.previewEV, -10.0f, 10.0f, T( Str::PreviewExposure ) ) ) hdr.previewDirty = true;
				ImGui::SameLine();
				if( ImGui::Button( T( Str::Auto ) ) ) { hdr.previewEV = hdr.preview.autoEV; hdr.previewDirty = true; }
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
			ImGui::SeparatorText( T( Str::SecBakeSettings ) );
			ImGui::BeginDisabled( running );
			{
				int device = s.useGpu ? 1 : 0;
				ImGui::TextUnformatted( T( Str::Processor ) );
				ImGui::SameLine();
				ImGui::RadioButton( "CPU", &device, 0 );
				ImGui::SameLine();
				ImGui::RadioButton( T( Str::GpuCompute ), &device, 1 );
				s.useGpu = ( device == 1 );
				if( s.useGpu ) TextDisabledWrapped( T( Str::GpuNote ) );
			}
			static const uint32_t srcSizes[] = { 128, 256, 512, 1024, 2048 };
			static const uint32_t pfSizes[]  = { 64, 128, 256, 512, 1024 };
			static const uint32_t lutSizes[] = { 64, 128, 256, 512 };
			static const uint32_t irrSizes[] = { 16, 32, 64, 128 };
			{
				static const ibl::CubeFormat formats[] = { ibl::CubeFormat::Float16, ibl::CubeFormat::Float32, ibl::CubeFormat::R11G11B10, ibl::CubeFormat::BC6H };
				const char* const hints[] = { T( Str::FormatF16 ), T( Str::FormatF32 ), T( Str::FormatR11 ), T( Str::FormatBc6h ) };
				int cur = 0;
				for( int i = 0; i < 4; ++i ) if( formats[i] == s.cubeFormat ) cur = i;
				static const char* const shortNames[] = { "RGBA16F", "RGBA32F", "R11G11B10F", "BC6H" }; // 表示部は幅に収まる短い名前、一覧に説明
				if( ImGui::BeginCombo( T( Str::CubemapFormat ), shortNames[cur] ) )
				{
					for( int i = 0; i < 4; ++i )
						if( ImGui::Selectable( hints[i], i == cur ) ) s.cubeFormat = formats[i];
					ImGui::EndCombo();
				}
			}
			ComboSize( T( Str::SourceCubeSize ), s.sourceCubeSize, srcSizes, 5 );
			ComboSize( T( Str::PrefilterSize ), s.prefilterSize, pfSizes, 5 );
			int ps = static_cast<int>( s.prefilterSamples );
			if( ImGui::InputInt( T( Str::PrefilterSamples ), &ps, 128, 1024 ) ) s.prefilterSamples = static_cast<uint32_t>( std::clamp( ps, 16, 16384 ) );
			ComboSize( T( Str::IrradianceSize ), s.irradianceSize, irrSizes, 4 );
			int is = static_cast<int>( s.irradianceSamples );
			if( ImGui::InputInt( T( Str::IrradianceSamples ), &is, 256, 1024 ) ) s.irradianceSamples = static_cast<uint32_t>( std::clamp( is, 16, 16384 ) );
			ComboSize( T( Str::LutSize ), s.lutSize, lutSizes, 4 );
			int ls = static_cast<int>( s.lutSamples );
			if( ImGui::InputInt( T( Str::LutSamples ), &ls, 128, 1024 ) ) s.lutSamples = static_cast<uint32_t>( std::clamp( ls, 16, 16384 ) );
			ImGui::InputFloat( T( Str::SampleClamp ), &s.maxSampleValue, 10.0f, 100.0f, "%.1f" );
			s.maxSampleValue = std::max( 0.0f, s.maxSampleValue );
			ImGui::BeginDisabled( !ctx.model.hdr.Loaded() );
			if( ImGui::Button( T( Str::Bake ) ) ) ctx.workflow.StartBake();
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
			if( !r.hasBaked )        TextDisabledWrapped( T( Str::NothingToSave ) );
			else if( r.bakedSaved )  TextDisabledWrapped( T( Str::Saved ) );
			else                     TextColoredWrapped( ImVec4( 1.0f, 0.75f, 0.25f, 1.0f ), T( Str::NotSavedYet ) );

			if( running )
			{
				const BakeController& bake = ctx.workflow.Bake();
				char overlay[160];
				std::snprintf( overlay, sizeof( overlay ), "%s  %d%%", i18n::StageName( bake.Stage() ).c_str(), static_cast<int>( bake.Progress() * 100.0f ) );
				const ImGuiStyle& style = ImGui::GetStyle();
				const float cancelW = ImGui::CalcTextSize( T( Str::Cancel ) ).x + style.FramePadding.x * 2.0f;
				ImGui::ProgressBar( bake.Progress(), ImVec2( -( cancelW + style.ItemSpacing.x ), 0 ), overlay );
				ImGui::SameLine();
				ImGui::BeginDisabled( ctx.workflow.IsCancelling() );
				if( ImGui::Button( T( Str::Cancel ) ) ) ctx.workflow.CancelBake();
				ImGui::EndDisabled();
			}
			ImGui::TextWrapped( "%s", m.status.empty() ? T( Str::StatusInitial ) : m.status.c_str() );

			if( r.hasOutputs )
			{
				ImGui::SeparatorText( !r.hasBaked || r.bakedSaved ? T( Str::Files ) : T( Str::FilesPrevious ) );
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
		ImGui::PushItemWidth( -LabelColumnWidth( { T( Str::CubemapFormat ), T( Str::SourceCubeSize ), T( Str::PrefilterSize ), T( Str::PrefilterSamples ),
			T( Str::IrradianceSize ), T( Str::IrradianceSamples ), T( Str::LutSize ), T( Str::LutSamples ), T( Str::SampleClamp ) }, ctx.dpiScale ) ); // 以降の項目: 右にラベル用の余白を確保
		DrawSettingsSection( ctx, running );
		DrawResultSection( ctx, running );
		ImGui::PopItemWidth();
	}
} // namespace ui
