#include "engine/graphics/camera/camera.h"
#include <algorithm>

void Camera::Rotate(const float yawChange, const float pitchChange)
{
	yaw += yawChange;
	pitch = std::clamp(pitch + pitchChange, -89.0f, 89.0f);
}

float Camera::GetYaw() const
{
	return yaw;
}

void Camera::SetPosition(const float x, const float y, const float z)
{
	position = {x, y, z};
}

DirectX::XMMATRIX Camera::GetViewMatrix() const
{
	using namespace DirectX;


	const XMVECTOR eye = XMLoadFloat3(&position);

	const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(pitch),
		XMConvertToRadians(yaw),
		0.0f
	);

	const XMVECTOR forward = XMVector3TransformNormal(
		XMVectorSet(0, 0, 1, 0),
		rotation
	);

	const XMVECTOR up = XMVector3TransformNormal(
		XMVectorSet(0, 1, 0, 0),
		rotation
	);

	return XMMatrixLookToLH(eye, forward, up);
}

DirectX::XMMATRIX Camera::GetProjectionMatrix(const float aspectRatio, const float farPlane) const
{
	return DirectX::XMMatrixPerspectiveFovLH(
		DirectX::XMConvertToRadians(fov), aspectRatio, 0.1f, farPlane
	);
}

DirectX::BoundingFrustum Camera::GetFrustum(const float aspectRatio, const float farPlane) const
{
	DirectX::BoundingFrustum frustum;
	DirectX::BoundingFrustum::CreateFromMatrix(frustum, GetProjectionMatrix(aspectRatio, farPlane));
	// Projection describes view space; culling bounds are in world space.
	frustum.Transform(frustum, DirectX::XMMatrixInverse(nullptr, GetViewMatrix()));
	return frustum;
}
