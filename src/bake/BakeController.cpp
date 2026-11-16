#include "BakeController.h"

#include <chrono>

BakeController::~BakeController()
{
	m_cancel.store( true );
	if( m_thread.joinable() ) m_thread.join();
}

bool BakeController::Start( ibl::BakeJob job )
{
	if( m_state.load() == State::Running ) return false;
	if( m_thread.joinable() ) m_thread.join();
	m_cancel.store( false );
	m_progress.store( 0.0f );
	m_stage.store( "Starting" );
	m_state.store( State::Running );
	m_thread = std::thread( [this, job = std::move( job )]() {
		ibl::BakedTextures out;
		std::string err;
		const auto t0 = std::chrono::steady_clock::now();
		const bool ok = ibl::RunBake( job, [this]( const char* stage, float p ) {
			if( m_cancel.load() ) throw ibl::BakeCancelled(); // 進捗通知の箇所 (CPU の各行 / GPU の各バンド) で中断する
			m_stage.store( stage );
			m_progress.store( p );
		}, out, err );
		const double sec = std::chrono::duration<double>( std::chrono::steady_clock::now() - t0 ).count();
		{
			std::lock_guard<std::mutex> lock( m_mutex );
			m_outputs = out;
			m_error = err;
			m_seconds = sec;
			m_usedGpu = job.settings.useGpu;
		}
		m_state.store( ok ? State::Finished : ( m_cancel.load() ? State::Canceled : State::Failed ) );
	} );
	return true;
}

void BakeController::Take( ibl::BakedTextures& out, std::string& err, double& seconds, bool& usedGpu )
{
	if( m_thread.joinable() ) m_thread.join();
	std::lock_guard<std::mutex> lock( m_mutex );
	out = m_outputs;
	err = m_error;
	seconds = m_seconds;
	usedGpu = m_usedGpu;
	m_state.store( State::Idle );
}
