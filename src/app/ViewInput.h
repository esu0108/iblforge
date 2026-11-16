// ViewInput.h : マウス / キーボード入力 (ImGui の IO) を、カメラ操作とアプリの要求へ変換する
#pragma once
#include "app/AppModel.h"
#include "app/BakeWorkflow.h"
#include "app/OrbitCamera.h"

// 3D ビュー上で押し始めたドラッグだけを回転にする (パネルのスライダー操作などでは回らない)。ホイールはビュー上でだけズーム。
void ProcessCameraInput( OrbitCamera& camera, const ViewMouseState& mouse );

// TAB (UI の表示切替) / Ctrl+O / Ctrl+I / Ctrl+S。テキスト入力中は無効
void ProcessShortcuts( AppModel& model, const BakeWorkflow& workflow );
