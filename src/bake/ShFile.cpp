#include "ShFile.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ibl
{
	namespace
	{
		std::string Num( float v )
		{
			char b[32];
			std::snprintf( b, sizeof( b ), "%.9g", static_cast<double>( v ) );
			return b;
		}

		void WriteArray( std::ostream& o, const char* key, const SH9& sh, bool last )
		{
			o << "  \"" << key << "\": [\n";
			for( int k = 0; k < 9; ++k )
				o << "    [" << Num( sh.c[k].x ) << ", " << Num( sh.c[k].y ) << ", " << Num( sh.c[k].z ) << "]" << ( k < 8 ? "," : "" ) << "\n";
			o << "  ]" << ( last ? "" : "," ) << "\n";
		}
	} // namespace

	bool WriteShJson( const std::filesystem::path& path, const SH9& radiance, std::string& err )
	{
		std::ofstream f( path, std::ios::binary | std::ios::trunc );
		if( !f ) { err = "Cannot write SH file."; return false; }
		f << "{\n"
			 "  \"format\": \"iblforge-sh\",\n"
			 "  \"version\": 1,\n"
			 "  \"bands\": 3,\n"
			 "  \"coordinate_system\": \"left-handed, Y-up (Direct3D). Directions are (x, y, z); equirect centre = +Z\",\n"
			 "  \"basis\": \"real spherical harmonics, order [Y00, Y1-1, Y10, Y11, Y2-2, Y2-1, Y20, Y21, Y22] = [0.282095, 0.488603*y, 0.488603*z, 0.488603*x, 1.092548*x*y, 1.092548*y*z, 0.315392*(3*z*z-1), 1.092548*x*z, 0.546274*(x*x-y*y)] (unit direction)\",\n"
			 "  \"usage\": \"diffuse = albedo * max(0, sum_i irradiance_over_pi[i] * Y_i(N)). irradiance_over_pi is the same quantity as the *_irradiance.dds cubemap (cosine-convolved irradiance divided by pi). radiance is the raw projection of the HDRI (L(w) = sum_i radiance[i] * Y_i(w)).\",\n";
		WriteArray( f, "irradiance_over_pi", IrradianceOverPiSH( radiance ), false );
		WriteArray( f, "radiance", radiance, true );
		f << "}\n";
		f.flush();
		if( !f ) { err = "Failed to write SH file."; return false; }
		return true;
	}

	bool ReadShJson( const std::filesystem::path& path, SH9& radiance, std::string& err )
	{
		std::ifstream f( path, std::ios::binary );
		if( !f ) { err = "Cannot open SH file."; return false; }
		std::stringstream ss;
		ss << f.rdbuf();
		const std::string text = ss.str();

		// 自前のフォーマットなので簡易に読む: キー "radiance" の後ろの "[" から、数値 27 個 (区切りの [ ] , 空白は読み飛ばす)
		const size_t key = text.find( "\"radiance\"" );
		if( key == std::string::npos ) { err = "SH file has no \"radiance\" array."; return false; }
		const size_t open = text.find( '[', key + 10 );
		if( open == std::string::npos ) { err = "SH file is malformed."; return false; }
		const char* p = text.c_str() + open + 1; // 外側の [ の次
		float v[27];
		for( int i = 0; i < 27; ++i )
		{
			while( *p && ( *p == '[' || *p == ']' || *p == ',' || std::isspace( static_cast<unsigned char>( *p ) ) ) ) ++p;
			char* end = nullptr;
			v[i] = std::strtof( p, &end );
			if( end == p ) { err = "SH file needs 9 RGB coefficients."; return false; }
			p = end;
		}
		for( int k = 0; k < 9; ++k ) radiance.c[k] = { v[k * 3 + 0], v[k * 3 + 1], v[k * 3 + 2] };
		return true;
	}
} // namespace ibl
