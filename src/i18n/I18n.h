// I18n.h : UI の多言語化。文字列は I18nStrings.inc の表 (id + 各言語) にまとめてある。
//   使い方: i18n::Tr( i18n::Str::Bake ) / printf 形式なら i18n::Fmt( Str::Opened, name )
//   言語は File ではなく "Language" メニューで切り替え、exe の隣の language.txt に保存する (初回は OS の言語から決める)。
//   フォントは ImGuiBackend が言語に合わせて切り替える (CJK は Windows 標準フォントを使う)。
#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>

namespace i18n
{
	enum class Lang : int { En, Ja, ZhCN, Ko, Es, Fr, De, Ru, Count };

	enum class Str : int
	{
#define I18N_STR( id, en, ja, zh, ko, es, fr, de, ru ) id,
#include "i18n/I18nStrings.inc"
#undef I18N_STR
		Count
	};

	void Init();                       // language.txt を読む (無ければ OS の言語)。最初に 1 回呼ぶ
	Lang Current();
	void Set( Lang lang );             // 切り替えて language.txt に保存する
	const char* NativeName( Lang lang ); // 言語名 (その言語自身の表記)

	const char* Tr( Str id );          // 現在の言語の文字列 (UTF-8)
	inline const char* T( Str id ) { return Tr( id ); }

	// ベイク段階名 (BakeController::Stage の英語名) を現在の言語へ。未知の名前はそのまま返す
	std::string StageName( const char* englishName );

	// printf 形式の翻訳文字列を整形する
	inline std::string Vformat( const char* fmt, va_list args )
	{
		va_list copy;
		va_copy( copy, args );
		const int n = std::vsnprintf( nullptr, 0, fmt, copy );
		va_end( copy );
		if( n <= 0 ) return {};
		std::string s( static_cast<size_t>( n ), '\0' );
		std::vsnprintf( s.data(), s.size() + 1, fmt, args );
		return s;
	}
	inline std::string Fmt( Str id, ... )
	{
		va_list args;
		va_start( args, id );
		std::string s = Vformat( Tr( id ), args );
		va_end( args );
		return s;
	}
} // namespace i18n
