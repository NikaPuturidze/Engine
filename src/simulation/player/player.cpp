#include "simulation/player/player.h"
#include "simulation/world/world.h"
#include <cmath>
#include <stdexcept>

void Player::Spawn(const World& world)
{
    position.y = FindSpawnHeight(world);
    velocity = {};
    grounded = true;
    ResetFallTracking();
    sprinting = false;
}

float Player::FindSpawnHeight(const World& world) const
{
    const int x = static_cast<int>(std::floor(position.x));
    const int z = static_cast<int>(std::floor(position.z));
    for (int y = Chunk::Height - 1; y >= 0; --y)
        if (world.GetBlock(x, y, z) != Air) return static_cast<float>(y + 1);
    throw std::runtime_error("Spawn terrain is not loaded");
}

void Player::ResetFallTracking()
{
    fallDistance = 0;
    landingSound = FallSound::None;
    landingBlock = Air;
}

Player::Player(const Vec3 position) : Entity(position)
{
}

bool Player::PlaceBlock(World& world, const int x, const int y, const int z, const BlockId block) const
{
    if (block == Air || world.GetBlock(x, y, z) != Air) return false;
    if (IntersectsBlock({x, y, z})) return false;
    return world.SetBlock(x, y, z, block);
}

bool Player::IntersectsBlock(const BlockPosition block) const
{
    return position.x + HalfWidth > block.x && position.x - HalfWidth < block.x + 1.0f &&
        position.y + Height > block.y && position.y < block.y + 1.0f &&
        position.z + HalfWidth > block.z && position.z - HalfWidth < block.z + 1.0f;
}

void Player::Move(const float right, const float forward, const float yawDegrees, const float deltaTime, const bool sprint, const float speedMultiplier)
{
	sprinting = sprint && forward > 0.0f;
	movementYaw = yawDegrees;
	ApplyMovementInput(right, forward, yawDegrees,
		(grounded ? WalkAcceleration : AirAcceleration) * (sprinting ? SprintMultiplier : 1.0f) * speedMultiplier, deltaTime);
}

void Player::Update(const float deltaTime, const World& world)
{
    const float previousY = position.y;
    const bool wasGrounded = grounded;
    UpdatePhysics(deltaTime, world);
    TrackFallAndLanding(world, previousY, wasGrounded);
}

void Player::TrackFallAndLanding(const World& world, const float previousY, const bool wasGrounded)
{
    if (grounded)
    {
        if (!wasGrounded) RecordLanding(world);
        fallDistance = 0;
    }
    else if (gameMode == GameMode::Survival && position.y < previousY)
    {
        // As in Java, the final collision tick is not added to fall distance.
        fallDistance += previousY - position.y;
    }
}

void Player::RecordLanding(const World& world)
{
    landingSound = GetFallSound(fallDistance, gameMode);
    landingBlock = FindSupportingBlock(world);
    // The surface impact sound accompanies only a damaging Survival fall.
    if (landingSound == FallSound::None) landingBlock = Air;
}

BlockId Player::FindSupportingBlock(const World& world) const
{
    const int y = static_cast<int>(std::floor(position.y - 0.05f));
    auto block = world.GetBlock(static_cast<int>(std::floor(position.x)), y, static_cast<int>(std::floor(position.z)));
    // Feet can land on a block's edge while their center is over air.
    for (int z = static_cast<int>(std::floor(position.z - HalfWidth));
        block == Air && z < static_cast<int>(std::ceil(position.z + HalfWidth)); ++z)
        for (int x = static_cast<int>(std::floor(position.x - HalfWidth));
            block == Air && x < static_cast<int>(std::ceil(position.x + HalfWidth)); ++x)
            block = world.GetBlock(x, y, z);
    return block;
}

void Player::Jump()
{
	if (!grounded) return;
	velocity.y = JumpVelocityPerTick * TicksPerSecond;
	if (sprinting)
	{
		const float yaw = Radians(movementYaw);
		velocity.x += std::sin(yaw) * SprintJumpBoostPerTick * TicksPerSecond;
		velocity.z += std::cos(yaw) * SprintJumpBoostPerTick * TicksPerSecond;
	}
	grounded = false;
}
