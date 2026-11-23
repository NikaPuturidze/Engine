#pragma once

#include "chunk.h"

class WorldGenerator
{
	public:
		virtual ~WorldGenerator() = default;

		virtual void Generate(Chunk& chunk) const = 0;
};
