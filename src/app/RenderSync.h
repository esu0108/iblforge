// RenderSync.h : AppModel の表示設定を Renderer へ反映する (リソースの作り直しを伴うのでフレーム外で呼ぶ)
#pragma once
#include "app/AppModel.h"
#include "gfx/Renderer.h"

// アンチエイリアス (TAA) の有効 / 無効と、HDRI プレビュー (トーンマップ → テクスチャ) を同期する
void SyncRenderer( AppModel& model, Renderer& renderer );
