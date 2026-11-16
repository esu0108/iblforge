// ParallelFor.h : 0..count-1 を複数スレッドへ振り分けて実行する (CPU ベイク用)
#pragma once
#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace ibl::detail
{
template <class F>
void ParallelFor( uint32_t count, uint32_t threadCount, F&& fn, const std::function<void( float )>& onProgress = {} )
{
	if( count == 0 ) return;
	if( threadCount == 0 ) threadCount = std::max( 1u, std::thread::hardware_concurrency() );
	threadCount = std::min( threadCount, count );

	std::atomic<uint32_t> next{ 0 };
	std::atomic<uint32_t> done{ 0 };
	// fn / onProgress が例外を投げたら (キャンセルを含む) 他のワーカーを止め、最初の例外を呼び出し元スレッドへ再送出する。
	std::exception_ptr error;
	std::mutex errorMutex;
	std::atomic<bool> failed{ false };
	auto worker = [&]() {
		try
		{
			for( ; !failed.load(); )
			{
				const uint32_t i = next.fetch_add( 1 );
				if( i >= count ) break;
				fn( i );
				const uint32_t d = done.fetch_add( 1 ) + 1;
				if( onProgress ) onProgress( static_cast<float>( d ) / static_cast<float>( count ) );
			}
		}
		catch( ... )
		{
			std::lock_guard<std::mutex> lock( errorMutex );
			if( !error ) error = std::current_exception();
			failed.store( true );
		}
	};
	std::vector<std::thread> pool;
	for( uint32_t t = 1; t < threadCount; ++t ) pool.emplace_back( worker );
	worker();
	for( auto& th : pool ) th.join();
	if( error ) std::rethrow_exception( error );
}
} // namespace ibl::detail
