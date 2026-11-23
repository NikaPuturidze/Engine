#include "simulation/physics/entity.h"
#include "simulation/world/world.h"
#include <algorithm>
#include <array>
#include <cmath>


namespace
{
struct CollisionCells
{
    std::array<int, 3> first;
    std::array<int, 3> last;
};

 std::array<float, 3> Components(const Vec3 value)
{
    return {value.x, value.y, value.z};
}

 CollisionCells FindSweptCells(const Bounds& bounds, const int axis, const float distance)
{
    const auto minimum = Components(bounds.minimum);
    const auto maximum = Components(bounds.maximum);
    CollisionCells cells;
    for (int i = 0; i < 3; ++i)
    {
        const float offset = i == axis ? distance : 0.0f;
        cells.first[i] = static_cast<int>(std::floor(minimum[i] + std::min(offset, 0.0f)));
        cells.last[i] = static_cast<int>(std::ceil(maximum[i] + std::max(offset, 0.0f))) - 1;
    }
    cells.first[1] = std::max(cells.first[1], 0);
    cells.last[1] = std::min(cells.last[1], Chunk::Height - 1);
    return cells;
}

 bool OverlapsOtherAxes(const Bounds& bounds, const std::array<float, 3>& block, const int axis)
{
    constexpr float tolerance = 0.00001f;
    const auto minimum = Components(bounds.minimum);
    const auto maximum = Components(bounds.maximum);
    for (int i = 0; i < 3; ++i)
    {
        if (i != axis && (maximum[i] <= block[i] + tolerance || minimum[i] >= block[i] + 1.0f - tolerance))
            return false;
    }
    return true;
}

 float LimitTravelAtBlock(const Bounds& bounds, const std::array<float, 3>& block,
    const int axis, const float distance, const float allowed)
{
    constexpr float tolerance = 0.00001f;
    const auto minimum = Components(bounds.minimum);
    const auto maximum = Components(bounds.maximum);
    if (!OverlapsOtherAxes(bounds, block, axis)) return allowed;
    if (distance > 0 && maximum[axis] <= block[axis] + tolerance)
        return std::min(allowed, std::max(0.0f, block[axis] - maximum[axis]));
    if (distance < 0 && minimum[axis] >= block[axis] + 1.0f - tolerance)
        return std::max(allowed, std::min(0.0f, block[axis] + 1.0f - minimum[axis]));
    return allowed;
}

 float FindAllowedTravel(const World& world, const Bounds& bounds, const int axis, const float distance)
{
    const auto cells = FindSweptCells(bounds, axis, distance);
    float allowed = distance;
    for (int y = cells.first[1]; y <= cells.last[1]; ++y)
        for (int z = cells.first[2]; z <= cells.last[2]; ++z)
            for (int x = cells.first[0]; x <= cells.last[0]; ++x)
            {
                if (world.TryGetBlock(x, y, z) == std::optional<BlockId>{Air}) continue;
                const std::array block{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                allowed = LimitTravelAtBlock(bounds, block, axis, distance, allowed);
            }
    return allowed;
}
}


void Entity::ApplyMovementInput(float right, float forward, const float yawDegrees, const float acceleration, const float deltaTime)
{
	if (const float length = std::sqrt(right * right + forward * forward); length > 1.0f)
	{
		right /= length;
		forward /= length;
	}

	constexpr float inputMultiplier = 0.98f;
	const float yaw = Radians(yawDegrees);
	const float velocityChange = acceleration * inputMultiplier * TicksPerSecond * TicksPerSecond * deltaTime;

	velocity.x += (right * std::cos(yaw) + forward * std::sin(yaw)) * velocityChange;
	velocity.z += (forward * std::cos(yaw) - right * std::sin(yaw)) * velocityChange;
}

void Entity::UpdatePhysics(const float deltaTime, const World& world)
{
    MoveHorizontally(deltaTime, world);
    ApplyHorizontalDrag(deltaTime);
    MoveVertically(deltaTime, world);
    ApplyGravity(deltaTime);
}

void Entity::MoveHorizontally(const float deltaTime, const World& world)
{
    MoveAlongAxis(0, velocity.x * deltaTime, world);
    MoveAlongAxis(2, velocity.z * deltaTime, world);
}

void Entity::ApplyHorizontalDrag(const float deltaTime)
{
    // Ground friction and air drag are alternatives, not cumulative forces.
    const float drag = std::pow(
        grounded ? GroundFrictionMultiplier : HorizontalDragPerTick,
        TicksPerSecond * deltaTime
    );
    velocity.x *= drag;
    velocity.z *= drag;
}

void Entity::MoveVertically(const float deltaTime, const World& world)
{
    // Move before gravity: the first displacement of a jump remains 0.42 blocks.
    if (std::abs(velocity.y) < 0.003f * TicksPerSecond) velocity.y = 0;
    if (velocity.y != 0) grounded = false;
    MoveAlongAxis(1, velocity.y * deltaTime, world);
}

void Entity::ApplyGravity(const float deltaTime)
{
    const float gravityChange = GravityAcceleration * TicksPerSecond * TicksPerSecond * deltaTime;
    velocity.y = (velocity.y - gravityChange) * std::pow(VerticalDragPerTick, TicksPerSecond * deltaTime);
}

void Entity::MoveAlongAxis(const int axis, const float distance, const World& world)
{
    if (distance == 0) return;
    const float allowed = FindAllowedTravel(world, GetBounds(), axis, distance);
    ApplyAxisMovement(axis, distance, allowed);
}

void Entity::ApplyAxisMovement(const int axis, const float distance, const float allowed)
{
    if (axis == 0)
    {
        position.x += allowed;
        if (allowed != distance) velocity.x = 0;
    }
    if (axis == 1)
    {
        position.y += allowed;
        if (allowed != distance)
        {
            grounded = distance < 0;
            velocity.y = 0;
        }
    }
    if (axis == 2)
    {
        position.z += allowed;
        if (allowed != distance) velocity.z = 0;
    }
}
