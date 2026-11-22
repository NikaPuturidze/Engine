#include "player.h"
#include <cmath>

Player::Player(const DirectX::XMFLOAT3 position) : position(position)
{
}

void Player::Move(float right, float forward, const float yawDegrees, const float deltaTime)
{
	if (const float length = std::sqrt(right * right + forward * forward); length > 1.0f)
	{
		right /= length;
		forward /= length;
	}

	constexpr float speed = 4.317f;

	const float yaw = DirectX::XMConvertToRadians(yawDegrees);
	const float distance = speed * deltaTime;

	position.x += (right * std::cos(yaw) + forward * std::sin(yaw)) * distance;
	position.z += (forward * std::cos(yaw) - right * std::sin(yaw)) * distance;
}

DirectX::XMFLOAT3 Player::GetPosition() const
{
	return position;
}
