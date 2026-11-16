// EventLoop.h : イベント駆動のメッセージループ
#pragma once
#include "app/App.h"

// 描画は「必要なときだけ」行う。
//   描画する条件: ウィンドウメッセージ (入力・リサイズ・再描画要求・ドロップなど) を受け取った / ベイク・読み込み中 (進捗表示) /
//                 テキスト入力中 (カーソル点滅) / 連続描画オン / 入力直後の数フレーム。
//   それ以外は MsgWaitForMultipleObjectsEx で OS に任せて眠る (CPU/GPU 使用率はほぼ 0)。
// WM_QUIT を受け取ったら戻る。
void RunEventLoop( App& app );
