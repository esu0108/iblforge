#include "AsyncHdrLoader.h"

#include <chrono>

#include "IBLIO.h"

bool AsyncHdrLoader::Start( const std::filesystem::path& path )
{
	if( IsLoading() ) return false;
	m_future = std::async( std::launch::async, [path]() {
		HdrLoadResult r;
		r.path = path;
		auto img = std::make_shared<ibl::EquirectImage>();
		r.ok = ibl::LoadEquirectHdr( path, *img, r.err );
		if( r.ok ) r.preview = ibl::BuildPreviewSource( *img );
		r.image = std::move( img );
		return r;
	} );
	return true;
}

bool AsyncHdrLoader::TryTake( HdrLoadResult& out )
{
	if( !m_future.valid() || m_future.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready ) return false;
	out = m_future.get();
	return true;
}
