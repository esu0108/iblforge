#include "SampleTableBuffer.h"

#include <algorithm>

namespace ibl
{
	namespace gpu
	{
		SampleTableBuffer SampleTableBuffer::Build( const BakeSettings& s, UINT srcSize, UINT srcMips, UINT pfMips )
		{
			SampleTableBuffer t;
			t.irrCount = std::max( s.irradianceSamples, 16u );
			t.pfOffset.assign( pfMips, 0 );
			t.pfCount.assign( pfMips, 0 );

			auto append = [&]( const std::vector<SampleDir>& v ) {
				for( const SampleDir& d : v ) t.data.push_back( { d.L.x, d.L.y, d.L.z, d.mip } );
			};
			t.irrOffset = static_cast<uint32_t>( t.data.size() );
			append( BuildIrradianceSampleTable( t.irrCount, srcSize, srcMips ) );
			for( UINT m = 1; m < pfMips; ++m )
			{
				const float roughness = static_cast<float>( m ) / static_cast<float>( pfMips - 1 );
				t.pfOffset[m] = static_cast<uint32_t>( t.data.size() );
				const auto v = BuildPrefilterSampleTable( roughness, s.prefilterSamples, srcSize, srcMips );
				t.pfCount[m] = static_cast<uint32_t>( v.size() );
				append( v );
			}
			return t;
		}
	} // namespace gpu
} // namespace ibl
