#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace util
{
	inline std::wstring Utf8ToWide( const std::string& s )
	{
		if( s.empty() ) return {};
		const int n = MultiByteToWideChar( CP_UTF8, 0, s.data(), static_cast<int>( s.size() ), nullptr, 0 );
		std::wstring w( static_cast<size_t>( n ), L'\0' );
		MultiByteToWideChar( CP_UTF8, 0, s.data(), static_cast<int>( s.size() ), w.data(), n );
		return w;
	}

	inline std::string WideToUtf8( const std::wstring& w )
	{
		if( w.empty() ) return {};
		const int n = WideCharToMultiByte( CP_UTF8, 0, w.data(), static_cast<int>( w.size() ), nullptr, 0, nullptr, nullptr );
		std::string s( static_cast<size_t>( n ), '\0' );
		WideCharToMultiByte( CP_UTF8, 0, w.data(), static_cast<int>( w.size() ), s.data(), n, nullptr, nullptr );
		return s;
	}

	inline std::filesystem::path PathFromUtf8( const std::string& s ) { return std::filesystem::path( Utf8ToWide( s ) ); }
	inline std::string PathToUtf8( const std::filesystem::path& p ) { return WideToUtf8( p.wstring() ); }

	inline std::string HrToString( HRESULT hr )
	{
		char buf[32];
		std::snprintf( buf, sizeof( buf ), "0x%08X", static_cast<unsigned>( hr ) );
		return buf;
	}

	inline void ThrowIfFailed( HRESULT hr, const char* what )
	{
		if( FAILED( hr ) ) throw std::runtime_error( std::string( what ) + " (HRESULT " + HrToString( hr ) + ")" );
	}

	// exe のあるディレクトリ
	inline std::filesystem::path ExeDir()
	{
		wchar_t buf[MAX_PATH * 2];
		const DWORD n = GetModuleFileNameW( nullptr, buf, static_cast<DWORD>( std::size( buf ) ) );
		return std::filesystem::path( std::wstring( buf, n ) ).parent_path();
	}
} // namespace util
