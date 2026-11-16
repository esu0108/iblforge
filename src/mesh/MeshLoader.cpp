#include "MeshLoader.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "rapidobj/rapidobj.hpp"

namespace mesh
{
	namespace
	{
		struct V3
		{
			float x = 0, y = 0, z = 0;
		};

		V3 Sub( V3 a, V3 b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
		V3 Cross( V3 a, V3 b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
		V3 Normalized( V3 v )
		{
			const float l = std::sqrt( v.x * v.x + v.y * v.y + v.z * v.z );
			return l > 1e-20f ? V3{ v.x / l, v.y / l, v.z / l } : V3{ 0.0f, 1.0f, 0.0f };
		}
	} // namespace

	bool LoadObj( const std::filesystem::path& path, MeshData& out, std::string& err, const LoadOptions& options )
	{
		try
		{
			rapidobj::Result result = rapidobj::ParseFile( path, rapidobj::MaterialLibrary::Ignore() );
			if( result.error )
			{
				err = "rapidobj: " + result.error.code.message();
				if( result.error.line_num > 0 ) err += " (line " + std::to_string( result.error.line_num ) + ": " + result.error.line + ")";
				return false;
			}
			if( !rapidobj::Triangulate( result ) )
			{
				err = "rapidobj: triangulation failed: " + result.error.code.message();
				return false;
			}

			const auto& pos = result.attributes.positions;
			const auto& nrm = result.attributes.normals;
			const size_t posCount = pos.size() / 3, nrmCount = nrm.size() / 3;
			if( posCount == 0 ) { err = "The .obj has no vertices."; return false; }

			// ---- 全シェイプの三角形インデックスを集める
			std::vector<rapidobj::Index> tri;
			for( const rapidobj::Shape& s : result.shapes )
				tri.insert( tri.end(), s.mesh.indices.begin(), s.mesh.indices.end() );
			if( tri.empty() || tri.size() % 3 != 0 ) { err = "The .obj has no triangles."; return false; }
			for( const rapidobj::Index& i : tri )
			{
				if( i.position_index < 0 || static_cast<size_t>( i.position_index ) >= posCount ||
					( i.normal_index >= 0 && static_cast<size_t>( i.normal_index ) >= nrmCount ) )
				{
					err = "The .obj has an out-of-range index.";
					return false;
				}
			}
			auto P = [&]( int i ) { return V3{ pos[3 * static_cast<size_t>( i ) + 0], pos[3 * static_cast<size_t>( i ) + 1], pos[3 * static_cast<size_t>( i ) + 2] }; };

			// ---- 法線の無い頂点用に、位置ごとの面積重み付きスムース法線を作る
			const bool needGenerated = std::any_of( tri.begin(), tri.end(), []( const rapidobj::Index& i ) { return i.normal_index < 0; } );
			std::vector<V3> generated;
			if( needGenerated )
			{
				generated.assign( posCount, V3{} );
				for( size_t t = 0; t < tri.size(); t += 3 )
				{
					const V3 p0 = P( tri[t].position_index ), p1 = P( tri[t + 1].position_index ), p2 = P( tri[t + 2].position_index );
					const V3 n = Cross( Sub( p1, p0 ), Sub( p2, p0 ) ); // 長さ = 面積の 2 倍 (面積重み)
					for( int k = 0; k < 3; ++k )
					{
						V3& a = generated[static_cast<size_t>( tri[t + k].position_index )];
						a.x += n.x; a.y += n.y; a.z += n.z;
					}
				}
			}

			// ---- (position, normal) の組で頂点を共有
			out.vertices.clear();
			out.indices.clear();
			out.indices.reserve( tri.size() );
			std::unordered_map<uint64_t, uint32_t> remap;
			remap.reserve( posCount );
			for( const rapidobj::Index& i : tri )
			{
				const uint64_t key = ( static_cast<uint64_t>( i.position_index ) << 32 ) | static_cast<uint32_t>( i.normal_index + 1 );
				auto it = remap.find( key );
				if( it == remap.end() )
				{
					const V3 p = P( i.position_index );
					V3 n;
					if( i.normal_index >= 0 )
						n = Normalized( { nrm[3 * static_cast<size_t>( i.normal_index ) + 0], nrm[3 * static_cast<size_t>( i.normal_index ) + 1], nrm[3 * static_cast<size_t>( i.normal_index ) + 2] } );
					else
						n = Normalized( generated[static_cast<size_t>( i.position_index )] );
					it = remap.emplace( key, static_cast<uint32_t>( out.vertices.size() ) ).first;
					out.vertices.push_back( { p.x, p.y, p.z, n.x, n.y, n.z } );
				}
				out.indices.push_back( it->second );
			}

			// ---- 正規化: バウンディングボックス中心を原点へ、外接球の半径を 1 に
			V3 lo{ out.vertices[0].px, out.vertices[0].py, out.vertices[0].pz }, hi = lo;
			for( const Vertex& v : out.vertices )
			{
				lo = { std::min( lo.x, v.px ), std::min( lo.y, v.py ), std::min( lo.z, v.pz ) };
				hi = { std::max( hi.x, v.px ), std::max( hi.y, v.py ), std::max( hi.z, v.pz ) };
			}
			const V3 c{ 0.5f * ( lo.x + hi.x ), 0.5f * ( lo.y + hi.y ), 0.5f * ( lo.z + hi.z ) };
			float radius = 0.0f;
			for( const Vertex& v : out.vertices )
				radius = std::max( radius, std::sqrt( ( v.px - c.x ) * ( v.px - c.x ) + ( v.py - c.y ) * ( v.py - c.y ) + ( v.pz - c.z ) * ( v.pz - c.z ) ) );
			if( !( radius > 0.0f ) ) { err = "The .obj is degenerate (zero size)."; return false; }
			const float inv = 1.0f / radius;

			// ---- 右手系 -> 左手系 (Z 反転) と Y 軸まわりの回転を同時に適用
			const float yaw = options.yawDegrees * ( 3.14159265358979f / 180.0f );
			const float cs = std::cos( yaw ), sn = std::sin( yaw );
			const float pitch = options.pitchDegrees * ( 3.14159265358979f / 180.0f );
			const float cp = std::cos( pitch ), sp = std::sin( pitch );
			for( Vertex& v : out.vertices )
			{
				// Z 反転 -> X 軸まわり (pitch) -> Y 軸まわり (yaw)。法線も同じ回転 (純回転なので逆転置は不要)
				float x = ( v.px - c.x ) * inv, y = ( v.py - c.y ) * inv, z = -( v.pz - c.z ) * inv;
				float nx = v.nx, ny = v.ny, nz = -v.nz;
				{
					const float y2 = y * cp - z * sp, z2 = y * sp + z * cp;
					y = y2; z = z2;
					const float ny2 = ny * cp - nz * sp, nz2 = ny * sp + nz * cp;
					ny = ny2; nz = nz2;
				}
				v.px = x * cs + z * sn;
				v.py = y;
				v.pz = -x * sn + z * cs;
				v.nx = nx * cs + nz * sn;
				v.ny = ny;
				v.nz = -nx * sn + nz * cs;
			}
			return true;
		}
		catch( const std::exception& e )
		{
			err = std::string( "Exception while loading .obj: " ) + e.what();
			return false;
		}
	}
} // namespace mesh
