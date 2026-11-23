#pragma once

#include "simulation/world/world.h"
#include "api/math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

struct BlockHit
{
	BlockId block;
	int x, y, z;
	int normalX = 0, normalY = 0, normalZ = 0;
};


namespace raycast_detail
{
class BlockRay
{
public:
    BlockRay(const Vec3 origin, const Vec3 direction)
    {
        const std::array position{origin.x, origin.y, origin.z};
        const std::array ray{direction.x, direction.y, direction.z};
        for (int axis = 0; axis < 3; ++axis) InitializeAxis(axis, position[axis], ray[axis]);
    }

    std::optional<BlockId> Query(const World& world) const
    {
        return world.TryGetBlock(cell[0], cell[1], cell[2]);
    }

    BlockHit Hit(const BlockId block) const
    {
        return {
            .block = block,
            .x = cell[0],
            .y = cell[1],
            .z = cell[2],
            .normalX = normal[0],
            .normalY = normal[1],
            .normalZ = normal[2]
        };
    }

    bool Advance(const float maxDistance)
    {
        const float distance = std::min(next[0], std::min(next[1], next[2]));
        if (distance > maxDistance) return false;
        normal = {};
        bool entered = false;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (next[axis] != distance) continue;
            if (!entered)
            {
                normal[axis] = -step[axis];
                entered = true;
            }
            cell[axis] += step[axis];
            next[axis] += delta[axis];
        }
        return true;
    }

private:
    std::array<int, 3> cell{}, step{}, normal{};
    std::array<float, 3> next{}, delta{};

    void InitializeAxis(const int axis, const float position, const float direction)
    {
        cell[axis] = static_cast<int>(std::floor(position));
        step[axis] = direction > 0 ? 1 : -1;
        if (direction == 0)
        {
            next[axis] = delta[axis] = std::numeric_limits<float>::infinity();
            return;
        }
        delta[axis] = std::abs(1.0f / direction);
        next[axis] = (cell[axis] + (step[axis] > 0 ? 1.0f : 0.0f) - position) / direction;
    }
};
}

inline std::optional<BlockHit> RaycastBlock(
    const World& world, const Vec3 origin, const Vec3 direction, const float maxDistance)
{
    const float length = Length(direction);
    if (!(length > 0) || !std::isfinite(length) || !(maxDistance >= 0)) return std::nullopt;
    const Vec3 normalized{direction.x / length, direction.y / length, direction.z / length};
    raycast_detail::BlockRay ray(origin, normalized);
    do
    {
        const auto block = ray.Query(world);
        if (!block) return std::nullopt;
        if (*block != Air) return ray.Hit(*block);
    }
    while (ray.Advance(maxDistance));
    return std::nullopt;
}
