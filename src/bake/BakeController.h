// BakeController.h : ベイクを別スレッドで実行し、進捗と結果を受け渡す (Win32 / D3D12 非依存)
#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "IBLIO.h"

class BakeController
{
public:
	enum class State { Idle, Running, Finished, Failed, Canceled };

	~BakeController(); // 実行中ならキャンセルして終了を待つ

	bool Start( ibl::BakeJob job ); // 実行中なら false

	// 実行中のベイクにキャンセルを要求する (次の進捗通知の時点で中断される。完了は State::Canceled で分かる)
	void Cancel() { m_cancel.store( true ); }
	bool CancelRequested() const { return m_cancel.load(); }

	State GetState() const { return m_state.load(); }
	float Progress() const { return m_progress.load(); }
	const char* Stage() const { return m_stage.load(); }

	// Finished / Failed / Canceled のときに結果を受け取り Idle に戻す
	void Take( ibl::BakedTextures& out, std::string& err, double& seconds, bool& usedGpu );

private:
	std::thread m_thread;
	std::atomic<State> m_state{ State::Idle };
	std::atomic<bool> m_cancel{ false };
	std::atomic<float> m_progress{ 0.0f };
	std::atomic<const char*> m_stage{ "" };
	std::mutex m_mutex;
	ibl::BakedTextures m_outputs;
	std::string m_error;
	double m_seconds = 0.0;
	bool m_usedGpu = false;
};
