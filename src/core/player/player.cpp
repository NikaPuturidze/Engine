#include "player.h"

Player::Player(const DirectX::XMFLOAT3 position) : Entity(position)
{
}

void Player::Move(const float right, const float forward, const float yawDegrees, const float deltaTime)
{
	ApplyMovementInput(right, forward, yawDegrees,
		grounded ? WalkAcceleration : AirAcceleration, deltaTime);
}

void Player::Update(const float deltaTime, const World& world)
{
	UpdatePhysics(deltaTime, world);
}
