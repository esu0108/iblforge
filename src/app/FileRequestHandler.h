// FileRequestHandler.h : File メニュー / ショートカット / ドロップの要求 (AppModel::requests) をダイアログ経由で処理する
#pragma once
#include "Common.h"

#include "app/AppModel.h"
#include "app/BakeWorkflow.h"

class FileRequestHandler
{
public:
	// フレーム開始前に呼ぶ (ダイアログを開くため)。処理した要求はクリアする
	void Process( HWND owner, AppModel& model, BakeWorkflow& workflow );

private:
	static void ReleaseInputAfterDialog();
};
