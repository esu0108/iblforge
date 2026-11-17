// AppModel.h : アプリの状態 (純データ)。UI は読み書きし、BakeWorkflow / FileRequestHandler / RenderSync が更新・反映する。
#pragma once
#include <filesystem>
#include <memory>
#include <string>

#include "bake/HdrPreview.h"
#include "bake/IBLIO.h"
#include "gfx/SceneParams.h"

// 読み込み済みの HDRI (ベイクはこれに対して行う) と、その左パネル用プレビュー
struct HdrDocument
{
	std::shared_ptr<const ibl::EquirectImage> image;
	std::filesystem::path path;
	ibl::PreviewSource preview;
	float previewEV = 0.0f;
	bool previewDirty = false;  // プレビューの再トーンマップ + 再アップロードが必要 (RenderSync = フレーム外で行う)

	bool Loaded() const { return image != nullptr; }
};

// ベイク結果 (メモリ上) と、直近に保存 / インポートしたファイル
struct BakeResults
{
	ibl::BakedTextures baked;       // 直近のベイク結果。File > Save... で初めてファイルに保存する
	bool hasBaked = false;          // baked が有効 (= Save できる)
	bool bakedSaved = false;        // baked を既に保存済み
	ibl::BakeOutputs outputs;       // 直近に保存 / インポートしたファイル
	bool hasOutputs = false;
	std::filesystem::path saveDir;  // 保存先フォルダ選択ダイアログの初期位置 (前回の保存先)
};

// 表示・動作のオプション
struct ViewOptions
{
	bool vsync = true;
	bool taaEnabled = true;         // アンチエイリアス (TAA) の有効 / 無効
	bool alwaysRender = false;      // true: 操作が無くても毎フレーム描画する (ベンチマーク / FPS 確認用)。既定は操作があったときだけ描画
	float envRotationDeg = 0.0f;
	bool showUi = true;             // TAB で全パネルを隠して 3D をウィンドウ全体に表示
	bool showBakePanel = true;
	bool showMaterialPanel = true;
};

// 3D ビューパネル上のマウス状態 (UI が書き、カメラ操作が読む)
struct ViewMouseState
{
	bool hovered = false;           // マウスが 3D ビュー上 (ズーム可)
	bool active = false;            // 3D ビュー上でドラッグ中 (回転可)
};

// メニュー / ショートカット / ドロップからの要求。ダイアログと GPU リソースの入れ替えは ImGui のフレーム外で行う
struct UserRequests
{
	bool open = false, import = false, save = false;
	std::wstring droppedFile;
};

struct AppModel
{
	ibl::BakeSettings settings;
	ibl::BakePreset bakePreset = ibl::BakePreset::Medium; // 既定の settings は Medium と一致。手動で値を変えると Custom になる
	std::string status;             // 空のときは UI が (言語に合わせた) 初期メッセージを表示する
	HdrDocument hdr;
	BakeResults results;
	SceneParams scene;
	ViewOptions view;
	ViewMouseState viewMouse;
	UserRequests requests;
};
