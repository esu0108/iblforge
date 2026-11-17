// UiLayout.h : ImGui のドッキングレイアウト (プラットフォーム非依存。ImGui の docking ブランチが必要)
//
//   +--------------+------------------------+------------------+
//   | IBL Bake     |        3D View         | Material /       |
//   | (入力・設定) |   (中央: シーン描画)   | Lighting         |
//   +--------------+------------------------+------------------+
//
//   - 画面全体を 1 つのドックスペースにし、3 つのパネルを初期配置する (DockBuilder)。
//   - 配置はユーザーが自由に変更でき、imgui.ini に保存される。ini に配置が残っていればそれを優先する。
//   - 3D は「3D View」パネルの内側の矩形に描画する。パネルは背景を持たないので、先に描いたシーンが透けて見える。
#pragma once
#include <imgui.h>

namespace ui
{
	// パネル。タイトルは言語に追従するが、ImGui のウィンドウ ID / docking の配置は "###" 以降 (固定) だけで決まる
	enum class Panel { Bake, View, Material };
	const char* PanelTitle( Panel p ); // ImGui::Begin / DockBuilderDockWindow 用 ("<翻訳>###<固定 ID>")
	const char* PanelLabel( Panel p ); // 表示用 (メニュー項目や案内文)。翻訳のみ

	// ドックスペース (画面全体) を開始する。メインメニューバーを出した「後」に呼ぶこと (作業領域から差し引かれるため)。
	// 初回 (ini に配置が無いとき) と resetLayout = true のときは既定の 3 ペイン配置を構築する。
	// 戻り値はドックスペースの ID。パネルの Begin() はこの呼び出し (DockHostEnd) の後に行う。
	ImGuiID DockHostBegin( float dpiScale, bool resetLayout );
	void    DockHostEnd();

	// 既定配置 (左 = 設定, 中央 = 3D, 右 = マテリアル) を構築する。DockHostBegin が内部で使う。
	void BuildDefaultLayout( ImGuiID dockspaceId, ImVec2 size, float dpiScale );

	// 3D View パネルの結果
	struct ViewInfo
	{
		float x = 0, y = 0, w = 0, h = 0;  // シーンを描く矩形 (ウィンドウのクライアント座標, ピクセル)
		bool  hovered = false;             // マウスが 3D ビュー上にある (ホイールでズームしてよい)
		bool  active = false;              // 3D ビュー上でボタンを押している最中 (ドラッグで回してよい)
	};

	// 「3D View」パネルを描く。シーンの描画先矩形とマウス状態を返す。
	// overlayText (任意) は左上に重ねて表示する (FPS など)。
	ViewInfo DrawViewPanel( float dpiScale, const char* overlayText );
} // namespace ui
