#include "CommandLine.h"

#include <cstdlib>

CmdLine ParseCmdLine( int argc, wchar_t** argv )
{
	CmdLine c;
	int positional = 0;
	for( int i = 1; i < argc; ++i )
	{
		const std::wstring a = argv[i];
		auto next = [&]() -> std::wstring { return ( i + 1 < argc ) ? std::wstring( argv[++i] ) : std::wstring(); };
		static const wchar_t* const kBakeOptions[] = { L"--src", L"--size", L"--samples", L"--lut", L"--lut-samples", L"--irr", L"--irr-samples", L"--clamp", L"--format", L"--gpu", L"--cpu" };
		for( const wchar_t* o : kBakeOptions ) if( a == o ) c.hasBakeOptions = true;
		if( a == L"--bake-only" )        c.bakeOnly = true;
		else if( a == L"--src" )         c.settings.sourceCubeSize   = static_cast<uint32_t>( _wtoi( next().c_str() ) );
		else if( a == L"--size" )        c.settings.prefilterSize    = static_cast<uint32_t>( _wtoi( next().c_str() ) );
		else if( a == L"--samples" )     c.settings.prefilterSamples = static_cast<uint32_t>( _wtoi( next().c_str() ) );
		else if( a == L"--lut" )         c.settings.lutSize          = static_cast<uint32_t>( _wtoi( next().c_str() ) );
		else if( a == L"--lut-samples" ) c.settings.lutSamples       = static_cast<uint32_t>( _wtoi( next().c_str() ) );
		else if( a == L"--irr" )         c.settings.irradianceSize    = static_cast<uint32_t>( _wtoi( next().c_str() ) );
		else if( a == L"--irr-samples" ) c.settings.irradianceSamples = static_cast<uint32_t>( _wtoi( next().c_str() ) );
		else if( a == L"--clamp" )       c.settings.maxSampleValue   = static_cast<float>( _wtof( next().c_str() ) );
		else if( a == L"--format" )
		{
			const std::wstring f = next();
			if( f == L"f16" ) c.settings.cubeFormat = ibl::CubeFormat::Float16;
			else if( f == L"f32" ) c.settings.cubeFormat = ibl::CubeFormat::Float32;
			else if( f == L"r11g11b10" ) c.settings.cubeFormat = ibl::CubeFormat::R11G11B10;
			else if( f == L"bc6h" ) c.settings.cubeFormat = ibl::CubeFormat::BC6H;
		}
		else if( a == L"--gpu" )         c.settings.useGpu           = true;
		else if( a == L"--cpu" )         c.settings.useGpu           = false;
		else if( a == L"--out" )         c.output = next();
		else if( a.rfind( L"--", 0 ) == 0 ) { /* 未知のオプションは無視 */ }
		else if( positional == 0 )       { c.input = a; ++positional; }
		else if( positional == 1 )       { c.output = a; ++positional; }
	}
	return c;
}
