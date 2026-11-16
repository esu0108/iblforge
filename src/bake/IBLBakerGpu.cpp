// IBLBakerGpu.cpp : BakeOnGpu (公開 API)。実処理は bake/gpu/ 以下 (GpuBaker ほか) に分割してある。
#include "IBLBakerGpu.h"

#include <exception>

#include "gpu/GpuBaker.h"

namespace ibl
{
	bool BakeOnGpu( ID3D12Device* device, const EquirectImage& eq, const BakeSettings& s, const ProgressFn& progress, GpuBakeResult& out, std::string& err )
	{
		try
		{
			gpu::GpuBaker baker( device, progress );
			baker.Run( eq, s, out );
			return true;
		}
		catch( const BakeCancelled& )
		{
			throw; // キャンセルは失敗ではない
		}
		catch( const std::exception& e )
		{
			err = std::string( "GPU bake failed: " ) + e.what();
			return false;
		}
	}
} // namespace ibl
