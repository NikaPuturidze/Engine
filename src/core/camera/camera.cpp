#include "camera.h"
#include "../window.h"
#include <algorithm>
#include <windows.h>

Camera::Camera()
{
	ShowCursor(FALSE);
}

Camera::~Camera()
{
	ShowCursor(TRUE);
}

void Camera::Update(Window& window)
{
	long mouseX = 0;
	long mouseY = 0;
	window.ConsumeMouseMovement(mouseX, mouseY);

	constexpr float sensitivity = 0.1f;
	Rotate(mouseX * sensitivity, mouseY * sensitivity);
}

void Camera::SetPosition(float x, float y, float z)
{
    position = {x, y, z};
}

void Camera::SetRotation(float yawDegrees, float pitchDegrees)
{
    yaw = yawDegrees;
    pitch = pitchDegrees;
}

void Camera::Rotate(const float yawChange, const float pitchChange)
{
    yaw += yawChange;
    pitch = std::clamp(pitch + pitchChange, -89.0f, 89.0f);
}

DirectX::XMMATRIX Camera::GetViewMatrix() const
{
    using namespace DirectX;
    const XMVECTOR eye = XMLoadFloat3(&position);
    const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(pitch), XMConvertToRadians(yaw), 0.0f
    );
    const XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotation);
    const XMVECTOR up = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rotation);
    return XMMatrixLookToLH(eye, forward, up);
}

DirectX::XMMATRIX Camera::GetProjectionMatrix(const float aspectRatio) const
{
    return DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(fov), aspectRatio, 0.1f, 100.0f
    );
}
