#include "BakedFileNames.h"

#include <cwchar>
#include <cwctype>
#include <string>

namespace ibl
{
	namespace
	{
		bool EndsWithNoCase( const std::wstring& s, const wchar_t* suffix )
		{
			const size_t n = std::wcslen( suffix );
			if( s.size() <= n ) return false; // stem が空の場合は対象外
			for( size_t i = 0; i < n; ++i )
				if( std::towlower( s[s.size() - n + i] ) != std::towlower( suffix[i] ) ) return false;
			return true;
		}
	} // namespace

	DeriveResult DeriveBakedFiles( const std::filesystem::path& file, BakeOutputs& out )
	{
		static const wchar_t* kSuffixes[] = { L"_prefiltered.dds", L"_brdf_lut.dds", L"_irradiance.dds" };
		DeriveResult r;
		const std::wstring name = file.filename().wstring();
		for( const wchar_t* suffix : kSuffixes )
		{
			if( !EndsWithNoCase( name, suffix ) ) continue;
			out = BakeOutputs::ForStem( file.parent_path(), name.substr( 0, name.size() - std::wcslen( suffix ) ) );
			std::error_code ec;
			for( const auto* f : { &out.prefilteredCube, &out.brdfLut, &out.irradianceCube } )
				if( !std::filesystem::exists( *f, ec ) ) { r.status = DeriveResult::Status::MissingFile; r.missing = *f; return r; }
			r.status = DeriveResult::Status::Ok;
			return r;
		}
		return r; // NotBakedName
	}
} // namespace ibl
