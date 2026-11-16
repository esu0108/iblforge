// OrbitCamera.h : 原点を中心に周回するカメラ (入力は知らない。回転量 / ズーム量を受け取って view / proj を作るだけ)
#pragma once
#include "gfx/SceneParams.h"

class OrbitCamera
{
public:
	void Reset();
	void Orbit( float deltaXPixels, float deltaYPixels );  // ドラッグ量 (px)
	void Zoom( float wheelNotches );                       // ホイール量 (正 = 近づく)

	// scene の view / proj / cameraPos を更新する。aspect は 3D View パネルの矩形から求めた縦横比
	void Apply( SceneParams& scene, float aspect ) const;

private:
	float m_yaw = 0.0f, m_pitch = 0.15f, m_distance = 3.6f;
};
