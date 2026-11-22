#pragma once

#include <cstdint>

using BlockId = std::uint32_t;

class Cube
{
public:
	explicit Cube(const BlockId blockId) : blockId(blockId) {}

	const BlockId blockId;
    mutable float x = 0.0f;
    float y = 0.0f;
    float z = 4.0f;
    float size = 1.0f;
};
