#include "i18n/I18n.h"

#include <cstring>
#include <fstream>

#include "Common.h"

namespace i18n
{
	namespace
	{
		constexpr int kLangCount = static_cast<int>( Lang::Count );

		const char* const kTable[][kLangCount] = {
#define I18N_STR( id, en, ja, zh, ko, es, fr, de, ru ) { en, ja, zh, ko, es, fr, de, ru },
#include "i18n/I18nStrings.inc"
#undef I18N_STR
		};
		static_assert( sizeof( kTable ) / sizeof( kTable[0] ) == static_cast<size_t>( Str::Count ), "translation table" );

		const char* const kCodes[kLangCount]       = { "en", "ja", "zh-CN", "ko", "es", "fr", "de", "ru" };
		const char* const kNativeNames[kLangCount] = { "English", "日本語", "简体中文", "한국어", "Español", "Français", "Deutsch", "Русский" };

		Lang g_lang = Lang::En;

		std::filesystem::path SettingsPath() { return util::ExeDir() / L"language.txt"; }

		bool FromCode( const std::string& code, Lang& out )
		{
			for( int i = 0; i < kLangCount; ++i )
				if( code == kCodes[i] ) { out = static_cast<Lang>( i ); return true; }
			return false;
		}

		Lang DetectSystemLang()
		{
			wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
			if( GetUserDefaultLocaleName( name, LOCALE_NAME_MAX_LENGTH ) == 0 ) return Lang::En;
			const std::string loc = util::WideToUtf8( name ); // "ja-JP", "zh-CN", "zh-Hant-TW" など
			auto starts = [&]( const char* p ) { return loc.compare( 0, std::strlen( p ), p ) == 0; };
			if( starts( "ja" ) ) return Lang::Ja;
			if( starts( "zh" ) ) return ( loc.find( "TW" ) != std::string::npos || loc.find( "HK" ) != std::string::npos || loc.find( "MO" ) != std::string::npos || loc.find( "Hant" ) != std::string::npos ) ? Lang::En : Lang::ZhCN;
			if( starts( "ko" ) ) return Lang::Ko;
			if( starts( "es" ) ) return Lang::Es;
			if( starts( "fr" ) ) return Lang::Fr;
			if( starts( "de" ) ) return Lang::De;
			if( starts( "ru" ) ) return Lang::Ru;
			return Lang::En;
		}
	} // namespace

	void Init()
	{
		g_lang = DetectSystemLang();
		std::ifstream in( SettingsPath() );
		std::string code;
		Lang saved;
		if( in && std::getline( in, code ) )
		{
			while( !code.empty() && ( code.back() == '\r' || code.back() == ' ' ) ) code.pop_back();
			if( FromCode( code, saved ) ) g_lang = saved;
		}
	}

	Lang Current() { return g_lang; }

	void Set( Lang lang )
	{
		if( lang == g_lang ) return;
		g_lang = lang;
		std::ofstream out( SettingsPath(), std::ios::trunc );
		if( out ) out << kCodes[static_cast<int>( lang )] << "\n"; // 保存の失敗は無視する (次回起動時に OS の言語へ戻るだけ)
	}

	const char* NativeName( Lang lang ) { return kNativeNames[static_cast<int>( lang )]; }

	const char* Tr( Str id )
	{
		const int i = static_cast<int>( id );
		if( i < 0 || i >= static_cast<int>( Str::Count ) ) return "?";
		return kTable[i][static_cast<int>( g_lang )];
	}

	std::string StageName( const char* name )
	{
		if( !name || !name[0] ) return {};
		struct Map { const char* key; Str id; };
		static const Map maps[] = {
			{ "Equirect", Str::StageEquirect }, { "Generating mips", Str::StageMips }, { "Irradiance", Str::StageIrradiance },
			{ "Prefilter", Str::StagePrefilter }, { "BRDF", Str::StageBrdf }, { "Packing", Str::StagePacking }, { "Done", Str::StageDone } };
		for( const Map& m : maps )
		{
			if( std::strncmp( name, m.key, std::strlen( m.key ) ) != 0 ) continue;
			std::string s = Tr( m.id );
			if( std::strstr( name, "(GPU)" ) ) s += " (GPU)";
			return s;
		}
		return name;
	}
} // namespace i18n
