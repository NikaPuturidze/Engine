#pragma once
#include <cstdint>
using BlockId = std::uint32_t;
inline constexpr BlockId Air = 0;
struct BlockPosition { int x=0, y=0, z=0; bool operator==(const BlockPosition&) const = default; };
enum class PlaceResult { Placed, Occupied, TerrainUnavailable, OutOfReach, IntersectsPlayer, InvalidBlock };
