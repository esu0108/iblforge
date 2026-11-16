// BakeWorkflow.h : 「HDRI を開く → ベイク → 結果を表示 / 保存 / インポート」の流れ。
//   AppModel の HDRI / 結果を更新し、Renderer へ IBL をアップロードする。ダイアログや UI は知らない。
#pragma once
#include <filesystem>

#include "app/AppModel.h"
#include "bake/AsyncHdrLoader.h"
#include "bake/BakeController.h"
#include "gfx/Renderer.h"

class BakeWorkflow
{
public:
	BakeWorkflow( AppModel& model, Renderer& renderer ) : m_model( model ), m_renderer( renderer ) {}

	bool IsLoading() const { return m_loader.IsLoading(); }
	bool IsBaking() const { return m_bake.GetState() == BakeController::State::Running; }
	bool IsBusy() const { return m_bake.GetState() != BakeController::State::Idle || m_loader.IsLoading(); } // 進捗表示 / 完了の取りこぼし防止
	const BakeController& Bake() const { return m_bake; } // 進捗表示用

	// HDRI を別スレッドで読み込む。完了は Poll() で受け取る。thenBake = 読み込み完了後に自動でベイクする
	void OpenHdr( const std::filesystem::path& path, bool thenBake = false );
	// 読み込み済みの HDRI をベイクする
	void StartBake();
	// 実行中のベイクを中断する (結果は捨てられ、直前のベイク結果 / 表示はそのまま残る)
	void CancelBake() { m_bake.Cancel(); }
	bool IsCancelling() const { return IsBaking() && m_bake.CancelRequested(); }
	// 既存の DDS ファイルをインポートして表示に使う (ファイル由来なので Save 対象にはならない)
	void ImportFiles( const ibl::BakeOutputs& outputs );
	// ベイク結果を dir に 3 ファイルとして保存する
	void SaveTo( const std::filesystem::path& dir );

	// フレーム開始前に毎フレーム呼ぶ。完了した読み込み / ベイクを受け取ってモデルと GPU に反映する
	void Poll();

private:
	void OnLoadFinished( HdrLoadResult& r );
	void OnBakeFinished( BakeController::State state );
	bool ShowBaked( ibl::BakedTextures baked ); // ベイク直後のメモリ上の結果を表示に使う (ファイルは書かない)

	AppModel& m_model;
	Renderer& m_renderer;
	BakeController m_bake;
	AsyncHdrLoader m_loader;
	bool m_bakeAfterLoad = false;
};
