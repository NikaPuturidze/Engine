#pragma once

#include <DirectXMath/Inc/DirectXMath.h>

class Window;

class Camera
{
public:
	Camera();
	~Camera();

	void Update(Window& window);
	void SetPosition(float x, float y, float z);
	void SetRotation(float yaw, float pitch);
	void Rotate(float yawChange, float pitchChange);

	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix(float aspectRatio) const;

private:
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, -5.0f };

	float yaw = 0.0f;
	float pitch = 0.0f;
	float fov = 70.0f;

};
