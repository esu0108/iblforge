#include "app/OrbitCamera.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

void OrbitCamera::Reset()
{
	m_yaw = 0.0f;
	m_pitch = 0.15f;
	m_distance = 3.6f;
}

void OrbitCamera::Orbit( float dx, float dy )
{
	m_yaw -= dx * 0.006f;
	m_pitch = std::clamp( m_pitch + dy * 0.006f, -1.5f, 1.5f );
}

void OrbitCamera::Zoom( float wheel )
{
	m_distance = std::clamp( m_distance * std::pow( 0.9f, wheel ), 1.6f, 30.0f );
}

void OrbitCamera::Apply( SceneParams& scene, float aspect ) const
{
	const XMVECTOR eye = XMVectorSet( m_distance * std::cos( m_pitch ) * std::sin( m_yaw ), m_distance * std::sin( m_pitch ),
									 -m_distance * std::cos( m_pitch ) * std::cos( m_yaw ), 1.0f );
	const XMMATRIX view = XMMatrixLookAtLH( eye, XMVectorZero(), XMVectorSet( 0, 1, 0, 0 ) );
	const XMMATRIX proj = XMMatrixPerspectiveFovLH( XMConvertToRadians( 45.0f ), aspect, 0.1f, 100.0f );
	XMStoreFloat4x4( &scene.view, view );
	XMStoreFloat4x4( &scene.proj, proj );
	XMStoreFloat3( &scene.cameraPos, eye );
}
