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
			fallDistance = 0.0f;
			landingSound = FallSound::None;
			landingBlock = 0;
			sprinting = false;
			return;
		}
	throw std::runtime_error("Spawn terrain is not loaded");
}

Player::Player(const DirectX::XMFLOAT3 position) : Entity(position)
{
}

bool Player::PlaceBlock(World& world, const int x, const int y, const int z, const BlockId block) const
{
	if (block == 0 || world.GetBlock(x, y, z) != 0) return false;
	if (position.x + HalfWidth > x && position.x - HalfWidth < x + 1.0f &&
		position.y + Height > y && position.y < y + 1.0f &&
		position.z + HalfWidth > z && position.z - HalfWidth < z + 1.0f)
		return false;
	return world.SetBlock(x, y, z, block);
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
	const float previousY = position.y;
	const bool wasGrounded = grounded;
	UpdatePhysics(deltaTime, world);
	if (grounded)
	{
		if (!wasGrounded)
		{
			landingSound = GetFallSound(fallDistance, gameMode);
			const int y = static_cast<int>(std::floor(position.y - 0.05f));
			landingBlock = world.GetBlock(static_cast<int>(std::floor(position.x)), y,
				static_cast<int>(std::floor(position.z)));
			// Feet can land on a block's edge while their center is over air.
			for (int z = static_cast<int>(std::floor(position.z - HalfWidth));
				landingBlock == 0 && z < static_cast<int>(std::ceil(position.z + HalfWidth)); ++z)
				for (int x = static_cast<int>(std::floor(position.x - HalfWidth));
					landingBlock == 0 && x < static_cast<int>(std::ceil(position.x + HalfWidth)); ++x)
					landingBlock = world.GetBlock(x, y, z);
			// Vanilla plays the block impact sound only with a damaging fall.
			if (landingSound == FallSound::None) landingBlock = 0;
		}
		fallDistance = 0.0f;
	}
	else if (gameMode == GameMode::Survival && position.y < previousY)
	{
		// As in Java, the final collision tick is not added to fall distance.
		fallDistance += previousY - position.y;
	}
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
