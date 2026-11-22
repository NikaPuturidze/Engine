#pragma once

#include <DirectXMath.h>

class Player
{
	public:
		explicit Player(DirectX::XMFLOAT3 position = {0.0f, 0.0f, -5.0f});
		void Move(float right, float forward, float yawDegrees, float deltaTime);

		DirectX::XMFLOAT3 GetPosition() const;
	private:
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 velocity{0.0f, 0.0f, 0.0f};
};
