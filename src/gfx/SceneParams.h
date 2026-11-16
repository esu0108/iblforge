// SceneParams.h : 描画するシーンの内容 (オブジェクトの種類 / カメラ / マテリアル / 表示設定)。D3D12 に依存しない純データ。
#pragma once
#include <DirectXMath.h>

#include <algorithm>

// 表示するオブジェクトの種類 (UI の選択肢と MeshLibrary の内部配列の添字)
enum SceneObject : int
{
	// 先頭 (= 0) が既定の表示モデル。.obj は CMake が configure 時にダウンロードし、ビルド時に meshforge でバイナリ (.mesh) に変換して exe の隣の assets/models/ に置く。
	SceneObject_Bunny = 0,  // Stanford Bunny
	SceneObject_Armadillo,  // Armadillo
	SceneObject_HappyBuddha,// Happy Buddha
	SceneObject_Lucy,       // Lucy
	SceneObject_Sphere,     // 球体 (内蔵。モデルが読めないときの代替も兼ねる)
	SceneObject_Count
};

const char* SceneObjectName( int object ); // UI 表示名 (定義は MeshLibrary.cpp)

struct SceneParams
{
	int object = SceneObject_Bunny; // SceneObject

	// シーンの描画先矩形 (ウィンドウのクライアント座標, px)。ドッキングした「3D View」パネルの内側を指定する。viewW/viewH <= 0 ならウィンドウ全体。
	float viewX = 0.0f, viewY = 0.0f, viewW = 0.0f, viewH = 0.0f;

	DirectX::XMFLOAT4X4 view{};
	DirectX::XMFLOAT4X4 proj{};
	DirectX::XMFLOAT3   cameraPos{ 0, 0, -3 };

	DirectX::XMFLOAT3 albedo{ 1.0f, 1.0f, 1.0f };
	float metallic = 0.0f;
	float roughness = 0.5f;
	float reflectance = 0.5f;
	float diffuseIntensity = 1.0f;
	float specularIntensity = 1.0f;

	float exposureEV = 0.0f;
	float envRotation = 0.0f;  // ラジアン
	float skyBlur = 0.0f;  // 0..1 (プレフィルタ mip を roughness として引く)
	int   tonemap = 0;

	bool useDiffuse = true;
	bool useSpecular = true;
	bool multiScatter = false;
	bool showSky = true;
};

// シーンを描く矩形 (バックバッファ上のピクセル座標)。3D View パネルの内側をウィンドウ内にクランプしたもの。
struct SceneRect
{
	float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
	float Width() const { return x1 - x0; }
	float Height() const { return y1 - y0; }
	bool Valid() const { return Width() >= 1.0f && Height() >= 1.0f; } // パネルが潰れていると描かない
};

inline SceneRect ComputeSceneRect( const SceneParams& p, unsigned width, unsigned height )
{
	SceneRect r{ 0.0f, 0.0f, static_cast<float>( width ), static_cast<float>( height ) };
	if( p.viewW > 0.0f && p.viewH > 0.0f )
	{
		const float w = r.x1, h = r.y1;
		r.x0 = std::clamp( p.viewX, 0.0f, w );
		r.y0 = std::clamp( p.viewY, 0.0f, h );
		r.x1 = std::clamp( p.viewX + p.viewW, r.x0, w );
		r.y1 = std::clamp( p.viewY + p.viewH, r.y0, h );
	}
	return r;
}
