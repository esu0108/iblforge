// UiContext.h : UI の各パネルが使う参照の束 (パネルはこれ経由でモデルを読み書きし、操作は workflow / requests に伝える)
#pragma once
#include "Common.h"

#include "app/AppModel.h"
#include "app/BakeWorkflow.h"
#include "app/OrbitCamera.h"
#include "gfx/Renderer.h"

namespace ui
{
	struct UiContext
	{
		AppModel& model;
		Renderer& renderer;
		BakeWorkflow& workflow;
		OrbitCamera& camera;
		HWND hwnd;
		float dpiScale;
	};

	// ImGui の標準パネルを組み立てる (メニューバー + ドッキング + 3 パネル)。シーンの描画矩形とマウス状態を model に書く
	void BuildMainUi( UiContext& ctx );

	// 個別のパネル (BuildMainUi が呼ぶ)
	bool DrawMenuBar( UiContext& ctx );       // 戻り値 = "Reset layout" が選ばれた
	void DrawBakePanel( UiContext& ctx );     // 左: HDRI 表示・ベイク設定
	void DrawMaterialPanel( UiContext& ctx ); // 右: オブジェクト・マテリアル・IBL・表示
} // namespace ui
