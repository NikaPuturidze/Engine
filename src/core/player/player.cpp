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

	constexpr float acceleration = 0.1f;
	constexpr float inputMultiplier = 0.98f;
	constexpr float frictionMultiplier = 0.91f * 0.6f;
	constexpr float ticksPerSecond = 20.0f;

	const float yaw = DirectX::XMConvertToRadians(yawDegrees);
	const float velocityChange = acceleration * inputMultiplier * ticksPerSecond * ticksPerSecond * deltaTime;

	velocity.x += (right * std::cos(yaw) + forward * std::sin(yaw)) * velocityChange;
	velocity.z += (forward * std::cos(yaw) - right * std::sin(yaw)) * velocityChange;

	position.x += velocity.x * deltaTime;
	position.z += velocity.z * deltaTime;

	const float friction = std::pow(frictionMultiplier, deltaTime * ticksPerSecond);

	velocity.x *= friction;
	velocity.z *= friction;
}

DirectX::XMFLOAT3 Player::GetPosition() const
{
	return position;
}
