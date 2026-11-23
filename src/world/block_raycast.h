#pragma once

#include "world.h"
#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

struct BlockHit
{
	BlockId block;
	int x, y, z;
};

// Traverse voxel boundaries in distance order, including negative world coordinates.
inline std::optional<BlockHit> RaycastBlock(const World& world, DirectX::XMFLOAT3 origin,
                                           DirectX::XMFLOAT3 direction, float maxDistance)
{
	const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
	if (!(length > 0) || !std::isfinite(length) || !(maxDistance >= 0)) return std::nullopt;
	const std::array position{origin.x, origin.y, origin.z};
	const std::array ray{direction.x / length, direction.y / length, direction.z / length};
	std::array<int, 3> cell{}, step{};
	std::array<float, 3> next{}, delta{};
	for (int axis = 0; axis < 3; ++axis)
	{
		cell[axis] = static_cast<int>(std::floor(position[axis]));
		step[axis] = ray[axis] > 0 ? 1 : -1;
		if (ray[axis] == 0)
			next[axis] = delta[axis] = std::numeric_limits<float>::infinity();
		else
		{
			delta[axis] = std::abs(1.0f / ray[axis]);
			next[axis] = (cell[axis] + (step[axis] > 0 ? 1.0f : 0.0f) - position[axis]) / ray[axis];
		}
	}
	for (;;)
	{
		if (const auto block = world.GetBlock(cell[0], cell[1], cell[2]); block != 0)
			return BlockHit{block, cell[0], cell[1], cell[2]};
		const float distance = (std::min)(next[0], (std::min)(next[1], next[2]));
		if (distance > maxDistance) return std::nullopt;
		// Cross all tied axes together so corner-only contacts do not select a block.
		for (int axis = 0; axis < 3; ++axis)
			if (next[axis] == distance)
			{
				cell[axis] += step[axis];
				next[axis] += delta[axis];
			}
	}
}
