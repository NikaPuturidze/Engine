#pragma once
#include <DirectXMath.h>

class Camera
{
	public:
		void Rotate(float yawChange, float pitchChange);
		float GetYaw() const;
		void SetPosition(float x, float y, float z);

		DirectX::XMMATRIX GetViewMatrix() const;
		DirectX::XMMATRIX GetProjectionMatrix(float aspectRatio) const;
	private:
		DirectX::XMFLOAT3 position{0.0f, 0.0f, -5.0f};

		float yaw = 0.0f;
		float pitch = 0.0f;
		float fov = 70.0f;
};
