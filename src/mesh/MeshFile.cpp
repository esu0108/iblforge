#include "MeshFile.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace mesh
{
	namespace
	{
		static_assert( sizeof( Vertex ) == 24, "Vertex must be 6 floats without padding" );
		static_assert( sizeof( float ) == 4, "float must be 32-bit" );

		constexpr char kMagic[4] = { 'I', 'B', 'L', 'M' };

		struct Header
		{
			char     magic[4];
			uint32_t version;
			uint32_t vertexCount;
			uint32_t indexCount;
			uint32_t indexSize;   // 2 or 4
			uint32_t reserved[3]; // 0
		};
		static_assert( sizeof( Header ) == 32, "Header must be 32 bytes" );

		bool IsLittleEndian()
		{
			const uint16_t v = 1;
			uint8_t b;
			std::memcpy( &b, &v, 1 );
			return b == 1;
		}

		struct FileCloser { void operator()( std::FILE* f ) const { if( f ) std::fclose( f ); } };
		using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

		FilePtr OpenFile( const std::filesystem::path& path, const wchar_t* modeW, const char* modeA )
		{
#ifdef _WIN32
			( void )modeA;
			return FilePtr( _wfopen( path.c_str(), modeW ) );
#else
			( void )modeW;
			return FilePtr( std::fopen( path.string().c_str(), modeA ) );
#endif
		}
	} // namespace

	bool SaveMeshFile( const std::filesystem::path& path, const MeshData& data, std::string& err )
	{
		if( !IsLittleEndian() ) { err = "Big-endian hosts are not supported."; return false; }
		if( data.vertices.empty() || data.indices.empty() || data.indices.size() % 3 != 0 ) { err = "The mesh is empty or not a triangle list."; return false; }
		if( data.vertices.size() > 0xFFFFFFFFull || data.indices.size() > 0xFFFFFFFFull ) { err = "The mesh is too large."; return false; }
		for( const uint32_t i : data.indices )
			if( i >= data.vertices.size() ) { err = "The mesh has an out-of-range index."; return false; }

		const bool use16 = data.vertices.size() <= 65536;
		Header h{};
		std::memcpy( h.magic, kMagic, 4 );
		h.version = kMeshFileVersion;
		h.vertexCount = static_cast<uint32_t>( data.vertices.size() );
		h.indexCount = static_cast<uint32_t>( data.indices.size() );
		h.indexSize = use16 ? 2 : 4;

		std::vector<uint16_t> idx16;
		if( use16 ) idx16.assign( data.indices.begin(), data.indices.end() ); // 範囲は上で検証済み

		FilePtr f = OpenFile( path, L"wb", "wb" );
		if( !f ) { err = "Cannot open for writing: " + path.string(); return false; }
		bool ok = std::fwrite( &h, sizeof( h ), 1, f.get() ) == 1 &&
				  std::fwrite( data.vertices.data(), sizeof( Vertex ), data.vertices.size(), f.get() ) == data.vertices.size();
		if( ok )
			ok = use16 ? std::fwrite( idx16.data(), 2, idx16.size(), f.get() ) == idx16.size()
					   : std::fwrite( data.indices.data(), 4, data.indices.size(), f.get() ) == data.indices.size();
		if( !ok || std::fflush( f.get() ) != 0 ) { err = "Write failed: " + path.string(); return false; }
		return true;
	}

	bool LoadMeshFile( const std::filesystem::path& path, MeshBinary& out, std::string& err )
	{
		if( !IsLittleEndian() ) { err = "Big-endian hosts are not supported."; return false; }
		FilePtr f = OpenFile( path, L"rb", "rb" );
		if( !f ) { err = "Cannot open the file."; return false; }

		Header h{};
		if( std::fread( &h, sizeof( h ), 1, f.get() ) != 1 ) { err = "The file is too short (no header)."; return false; }
		if( std::memcmp( h.magic, kMagic, 4 ) != 0 ) { err = "Not a .mesh file (bad magic)."; return false; }
		if( h.version != kMeshFileVersion ) { err = "Unsupported .mesh version " + std::to_string( h.version ) + " (expected " + std::to_string( kMeshFileVersion ) + "). Rebuild to regenerate."; return false; }
		if( h.vertexCount == 0 || h.indexCount == 0 || h.indexCount % 3 != 0 || ( h.indexSize != 2 && h.indexSize != 4 ) ) { err = "Invalid .mesh header."; return false; }
		if( h.indexSize == 2 && h.vertexCount > 65536 ) { err = "Invalid .mesh header (16-bit indices with too many vertices)."; return false; }

		// 宣言されたサイズと実ファイルサイズが一致するか (切り詰め・余分なデータの検出)。64bit で計算してオーバーフローを避ける
		const uint64_t expected = sizeof( Header ) + static_cast<uint64_t>( h.vertexCount ) * sizeof( Vertex ) + static_cast<uint64_t>( h.indexCount ) * h.indexSize;
		std::error_code ec;
		const uint64_t actual = std::filesystem::file_size( path, ec );
		if( ec || actual != expected ) { err = "The file size does not match the header (truncated or corrupt)."; return false; }

		out.vertices.resize( h.vertexCount );
		out.indexData.resize( static_cast<size_t>( h.indexCount ) * h.indexSize );
		out.indexCount = h.indexCount;
		out.indexSize = h.indexSize;
		if( std::fread( out.vertices.data(), sizeof( Vertex ), h.vertexCount, f.get() ) != h.vertexCount ||
			std::fread( out.indexData.data(), h.indexSize, h.indexCount, f.get() ) != h.indexCount )
		{
			err = "Read failed.";
			return false;
		}

		// インデックスが頂点配列の範囲内か (GPU が範囲外を読まないように)
		for( uint32_t i = 0; i < h.indexCount; ++i )
		{
			uint32_t v;
			if( h.indexSize == 2 ) { uint16_t t; std::memcpy( &t, &out.indexData[static_cast<size_t>( i ) * 2], 2 ); v = t; }
			else std::memcpy( &v, &out.indexData[static_cast<size_t>( i ) * 4], 4 );
			if( v >= h.vertexCount ) { err = "The .mesh has an out-of-range index."; return false; }
		}
		return true;
	}
} // namespace mesh
