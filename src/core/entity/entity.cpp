#include "entity.h"
#include "../../world/world.h"
#include <algorithm>
#include <array>
#include <cmath>

void Entity::ApplyMovementInput(float right, float forward, const float yawDegrees, const float acceleration, const float deltaTime)
{
	if (const float length = std::sqrt(right * right + forward * forward); length > 1.0f)
	{
		right /= length;
		forward /= length;
	}

	constexpr float inputMultiplier = 0.98f;
	const float yaw = DirectX::XMConvertToRadians(yawDegrees);
	const float velocityChange = acceleration * inputMultiplier * TicksPerSecond * TicksPerSecond * deltaTime;

	velocity.x += (right * std::cos(yaw) + forward * std::sin(yaw)) * velocityChange;
	velocity.z += (forward * std::cos(yaw) - right * std::sin(yaw)) * velocityChange;
}

void Entity::UpdatePhysics(const float deltaTime, const World& world)
{
	const float gravityChange = GravityAcceleration * TicksPerSecond * TicksPerSecond * deltaTime;

	// Apply horizontal movement
	MoveAlongAxis(0, velocity.x * deltaTime, world);
	MoveAlongAxis(2, velocity.z * deltaTime, world);

	// Ground friction and air drag are alternatives, not cumulative forces.
	const float horizontalDrag = std::pow(
		grounded ? GroundFrictionMultiplier : HorizontalDragPerTick, TicksPerSecond * deltaTime);
	velocity.x *= horizontalDrag;
	velocity.z *= horizontalDrag;

	// Move with this tick's velocity before computing the next tick's velocity.
	// In particular, a jump's first displacement must be the full 0.42 blocks.
	if (std::abs(velocity.y) < 0.003f * TicksPerSecond) velocity.y = 0.0f;
	if (velocity.y != 0.0f) grounded = false;
	MoveAlongAxis(1, velocity.y * deltaTime, world);
	velocity.y = (velocity.y - gravityChange) * std::pow(VerticalDragPerTick, TicksPerSecond * deltaTime);
}

void Entity::MoveAlongAxis(const int axis, const float distance, const World& world)
{
	if (distance == 0.0f) return;

	// Position is the center of the feet. Blocks extend from integer corners to corner + 1.
	const std::array minimum{position.x - HalfWidth, position.y, position.z - HalfWidth};
	const std::array maximum{position.x + HalfWidth, position.y + Height, position.z + HalfWidth};

	std::array<int, 3> first{}, last{};
	for (int i = 0; i < 3; ++i)
	{
		const float offset = i == axis ? distance : 0.0f;
		first[i] = static_cast<int>(std::floor(minimum[i] + std::min(offset, 0.0f)));
		last[i] = static_cast<int>(std::ceil(maximum[i] + std::max(offset, 0.0f))) - 1;
	}
	first[1] = std::max(first[1], 0);
	last[1] = std::min(last[1], Chunk::Height - 1);

	float allowed = distance;

	for (int y = first[1]; y <= last[1]; ++y)
		for (int z = first[2]; z <= last[2]; ++z)
			for (int x = first[0]; x <= last[0]; ++x)
			{
				constexpr float contactTolerance = 0.00001f;

				if (world.GetBlock(x, y, z) == 0) continue;
				const std::array block{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};

				bool overlaps = true;
				for (int i = 0; i < 3; ++i)
					if (i != axis && (maximum[i] <= block[i] + contactTolerance || minimum[i] >= block[i] + 1.0f - contactTolerance))
						overlaps = false;

				if (!overlaps) continue;

				if (distance > 0.0f && maximum[axis] <= block[axis] + contactTolerance)
					allowed = std::min(allowed, std::max(0.0f, block[axis] - maximum[axis]));
				else if (distance < 0.0f && minimum[axis] >= block[axis] + 1.0f - contactTolerance)
					allowed = std::max(allowed, std::min(0.0f, block[axis] + 1.0f - minimum[axis]));
			}

	if (axis == 0) { position.x += allowed; if (allowed != distance) velocity.x = 0.0f; }
	if (axis == 1)
	{
		position.y += allowed;
		if (allowed != distance)
		{
			grounded = distance < 0.0f;
			velocity.y = 0.0f;
		}
	}
	if (axis == 2) { position.z += allowed; if (allowed != distance) velocity.z = 0.0f; }
}
