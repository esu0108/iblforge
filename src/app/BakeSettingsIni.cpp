#include "app/BakeSettingsIni.h"

#include <imgui.h>
#include <imgui_internal.h> // ImGuiSettingsHandler

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>

namespace bake_ini
{
	namespace
	{
		constexpr const char* kType = "IBLForge";
		constexpr const char* kSection = "Bake";

		struct PresetName { ibl::BakePreset p; const char* name; };
		constexpr PresetName kPresets[] = {
			{ ibl::BakePreset::Low, "Low" }, { ibl::BakePreset::Medium, "Medium" }, { ibl::BakePreset::High, "High" },
			{ ibl::BakePreset::Ultra, "Ultra" }, { ibl::BakePreset::Custom, "Custom" },
		};
		constexpr ibl::CubeFormat kFormats[] = { ibl::CubeFormat::Float16, ibl::CubeFormat::Float32, ibl::CubeFormat::R11G11B10, ibl::CubeFormat::BC6H };

		// UI (BakePanel.cpp) の選択肢と同じ範囲。それ以外の値は読み込まない (手で書き換えた ini で UI に無い値になるのを防ぐ)
		bool OneOf( uint32_t v, std::initializer_list<uint32_t> list )
		{
			for( uint32_t x : list ) if( x == v ) return true;
			return false;
		}

		struct State
		{
			AppModel* model = nullptr;
			bool applyLoaded = true;

			// 読み込み中の値 (ReadInit で現在値から始め、ApplyAll で反映する)
			ibl::BakeSettings loaded;
			ibl::BakePreset loadedPreset = ibl::BakePreset::Medium;
			bool any = false;

			// 前回「保存対象として確認した」値 (Sync で変化を検出する)
			ibl::BakeSettings last;
			ibl::BakePreset lastPreset = ibl::BakePreset::Medium;
		};
		State g;

		bool Same( const ibl::BakeSettings& a, const ibl::BakeSettings& b )
		{
			return ibl::SamePresetFields( a, b ) && a.maxSampleValue == b.maxSampleValue && a.cubeFormat == b.cubeFormat && a.useGpu == b.useGpu;
		}

		void ReadInit( ImGuiContext*, ImGuiSettingsHandler* )
		{
			g.loaded = g.model->settings;
			g.loadedPreset = g.model->bakePreset;
			g.any = false;
		}

		void* ReadOpen( ImGuiContext*, ImGuiSettingsHandler*, const char* name )
		{
			return std::strcmp( name, kSection ) == 0 ? &g : nullptr; // 他のセクション名は無視
		}

		void ReadLine( ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line )
		{
			const char* eq = std::strchr( line, '=' );
			if( !eq ) return;
			const std::string key( line, static_cast<size_t>( eq - line ) );
			const char* val = eq + 1;
			ibl::BakeSettings& s = g.loaded;
			const uint32_t u = static_cast<uint32_t>( std::strtoul( val, nullptr, 10 ) );

			if( key == "Preset" )
			{
				for( const PresetName& p : kPresets ) if( std::strcmp( val, p.name ) == 0 ) { g.loadedPreset = p.p; g.any = true; }
			}
			else if( key == "SourceCubeSize" )    { if( OneOf( u, { 128, 256, 512, 1024, 2048 } ) ) { s.sourceCubeSize = u; g.any = true; } }
			else if( key == "PrefilterSize" )     { if( OneOf( u, { 64, 128, 256, 512, 1024 } ) )   { s.prefilterSize = u; g.any = true; } }
			else if( key == "LutSize" )           { if( OneOf( u, { 64, 128, 256, 512 } ) )         { s.lutSize = u; g.any = true; } }
			else if( key == "IrradianceSize" )    { if( OneOf( u, { 16, 32, 64, 128 } ) )           { s.irradianceSize = u; g.any = true; } }
			else if( key == "PrefilterSamples" )  { if( u >= 16 && u <= 16384 ) { s.prefilterSamples = u; g.any = true; } }
			else if( key == "LutSamples" )        { if( u >= 16 && u <= 16384 ) { s.lutSamples = u; g.any = true; } }
			else if( key == "IrradianceSamples" ) { if( u >= 16 && u <= 16384 ) { s.irradianceSamples = u; g.any = true; } }
			else if( key == "SampleClamp" )
			{
				const float f = std::strtof( val, nullptr );
				if( f >= 0.0f && f < 1.0e9f ) { s.maxSampleValue = f; g.any = true; } // NaN / 負値は不可
			}
			else if( key == "CubemapFormat" )
			{
				for( ibl::CubeFormat f : kFormats ) if( std::strcmp( val, ibl::CubeFormatName( f ) ) == 0 ) { s.cubeFormat = f; g.any = true; }
			}
			else if( key == "Processor" )
			{
				if( std::strcmp( val, "GPU" ) == 0 ) { s.useGpu = true; g.any = true; }
				else if( std::strcmp( val, "CPU" ) == 0 ) { s.useGpu = false; g.any = true; }
			}
			// 未知のキーは無視 (新しい版の ini を古い版で読んでも壊れない)
		}

		void ApplyAll( ImGuiContext*, ImGuiSettingsHandler* )
		{
			if( !g.applyLoaded || !g.any ) return;
			ibl::BakeSettings s = g.loaded;
			s.threadCount = g.model->settings.threadCount;
			// プリセットは「値がそのプリセットと一致している」ときだけ有効。手で編集された ini などで食い違えば Custom
			ibl::BakePreset preset = g.loadedPreset;
			if( preset != ibl::BakePreset::Custom )
			{
				ibl::BakeSettings expect = s;
				ibl::ApplyBakePreset( expect, preset );
				if( !ibl::SamePresetFields( expect, s ) ) preset = ibl::BakePreset::Custom;
			}
			g.model->settings = s;
			g.model->bakePreset = preset;
			g.last = s;           // 読み込んだ直後の状態は「変更」ではない
			g.lastPreset = preset;
		}

		void WriteAll( ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf )
		{
			const ibl::BakeSettings& s = g.model->settings;
			const char* preset = "Custom";
			for( const PresetName& p : kPresets ) if( p.p == g.model->bakePreset ) preset = p.name;
			buf->appendf( "[%s][%s]\n", handler->TypeName, kSection );
			buf->appendf( "Preset=%s\n", preset );
			buf->appendf( "Processor=%s\n", s.useGpu ? "GPU" : "CPU" );
			buf->appendf( "CubemapFormat=%s\n", ibl::CubeFormatName( s.cubeFormat ) );
			buf->appendf( "SourceCubeSize=%u\n", s.sourceCubeSize );
			buf->appendf( "PrefilterSize=%u\n", s.prefilterSize );
			buf->appendf( "PrefilterSamples=%u\n", s.prefilterSamples );
			buf->appendf( "IrradianceSize=%u\n", s.irradianceSize );
			buf->appendf( "IrradianceSamples=%u\n", s.irradianceSamples );
			buf->appendf( "LutSize=%u\n", s.lutSize );
			buf->appendf( "LutSamples=%u\n", s.lutSamples );
			buf->appendf( "SampleClamp=%.3f\n", static_cast<double>( s.maxSampleValue ) );
			buf->append( "\n" );
		}
	} // namespace

	void Register( AppModel& model, bool applyLoaded )
	{
		g.model = &model;
		g.applyLoaded = applyLoaded;
		g.last = model.settings;
		g.lastPreset = model.bakePreset;

		ImGuiSettingsHandler h;
		h.TypeName = kType;
		h.TypeHash = ImHashStr( kType );
		h.ReadInitFn = ReadInit;
		h.ReadOpenFn = ReadOpen;
		h.ReadLineFn = ReadLine;
		h.ApplyAllFn = ApplyAll;
		h.WriteAllFn = WriteAll;
		ImGui::AddSettingsHandler( &h );
	}

	void Sync()
	{
		if( !g.model ) return;
		if( g.model->bakePreset != g.lastPreset || !Same( g.model->settings, g.last ) )
		{
			g.last = g.model->settings;
			g.lastPreset = g.model->bakePreset;
			ImGui::MarkIniSettingsDirty();
		}
	}
} // namespace bake_ini
