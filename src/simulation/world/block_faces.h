#pragma once

#include <cstdint>

using BlockFaceMask = std::uint8_t;

inline constexpr BlockFaceMask AllBlockFaces = 0x3f;

template <class IsSolid>
constexpr BlockFaceMask GetVisibleBlockFaces(
	const int x, const int y, const int z, IsSolid isSolid)
{
	constexpr int offsets[6][3] = {
		{0, 0, -1}, {0, 0, 1}, {-1, 0, 0},
		{1, 0, 0}, {0, 1, 0}, {0, -1, 0}
	};

	BlockFaceMask visible = 0;

	for (int face = 0; face < 6; ++face)
		if (!isSolid(x + offsets[face][0], y + offsets[face][1], z + offsets[face][2]))
			visible |= static_cast<BlockFaceMask>(1u << face);

	return visible;
}
