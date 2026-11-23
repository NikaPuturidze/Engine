#include "player.h"
#include "../../world/world.h"
#include <cmath>
#include <stdexcept>

void Player::Spawn(const World& world)
{
	const int x = static_cast<int>(std::floor(position.x));
	const int z = static_cast<int>(std::floor(position.z));
	for (int y = Chunk::Height - 1; y >= 0; --y)
		if (world.GetBlock(x, y, z) != 0)
		{
			position.y = static_cast<float>(y + 1);
			velocity = {0, 0, 0};
			grounded = true;
			sprinting = false;
			return;
		}
	throw std::runtime_error("Spawn terrain is not loaded");
}

Player::Player(const DirectX::XMFLOAT3 position) : Entity(position)
{
}

void Player::Move(const float right, const float forward, const float yawDegrees, const float deltaTime, const bool sprint)
{
	sprinting = sprint && forward > 0.0f;
	movementYaw = yawDegrees;
	ApplyMovementInput(right, forward, yawDegrees,
		(grounded ? WalkAcceleration : AirAcceleration) * (sprinting ? SprintMultiplier : 1.0f), deltaTime);
}

void Player::Update(const float deltaTime, const World& world)
{
	UpdatePhysics(deltaTime, world);
}

void Player::Jump()
{
	if (!grounded) return;
	velocity.y = JumpVelocityPerTick * TicksPerSecond;
	if (sprinting)
	{
		const float yaw = DirectX::XMConvertToRadians(movementYaw);
		velocity.x += std::sin(yaw) * SprintJumpBoostPerTick * TicksPerSecond;
		velocity.z += std::cos(yaw) * SprintJumpBoostPerTick * TicksPerSecond;
	}
	grounded = false;
}
