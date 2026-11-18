// BakeSettingsIni.h : ベイク設定 (品質プリセット + 詳細パラメータ) を imgui.ini に保存 / 復元する
//
//   ImGui の設定ハンドラ ([IBLForge][Bake] セクション) として登録する。パネル配置と同じ imgui.ini に載るので、
//   読み込みは最初の ImGui::NewFrame で (ImGui が) 行い、書き出しは設定が変わった後の自動保存 / 終了時に行われる。
//
//   保存するもの : 品質プリセット, 出力形式, 各サイズ / サンプル数, サンプルクランプ, 処理装置 (CPU / GPU)
//   保存しないもの: スレッド数 (UI が無い)
#pragma once
#include "app/AppModel.h"

namespace bake_ini
{
	// ImGui コンテキストの作成後、最初の NewFrame より前に呼ぶ。model は ImGui コンテキスト (Renderer) より後に破棄されること
	// (終了時の保存で参照する)。applyLoaded が false のときは、ini に書かれた値を読み込まず、現在の設定を保つ
	// (コマンドラインで明示された設定を優先したいとき)。保存は常に行う。
	void Register( AppModel& model, bool applyLoaded );

	// 毎フレーム呼ぶ。設定が変わっていたら ini を「要保存」にする (ImGui が少し後にまとめて書き出す)
	void Sync();
} // namespace bake_ini
