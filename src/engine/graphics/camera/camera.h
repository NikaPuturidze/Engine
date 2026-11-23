#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>

class Camera
{
	public:
		void Rotate(float yawChange, float pitchChange);
		float GetYaw() const;
		float GetPitch() const { return pitch; }
		DirectX::XMFLOAT3 GetPosition() const { return position; }
		void SetPosition(float x, float y, float z);

		DirectX::XMMATRIX GetViewMatrix() const;
		DirectX::XMMATRIX GetProjectionMatrix(float aspectRatio, float farPlane = 100.0f) const;
		DirectX::BoundingFrustum GetFrustum(float aspectRatio, float farPlane = 100.0f) const;
	private:
		DirectX::XMFLOAT3 position{0.0f, 0.0f, -5.0f};

		float yaw = 0.0f;
		float pitch = 0.0f;
		float fov = 90.0f;
};
