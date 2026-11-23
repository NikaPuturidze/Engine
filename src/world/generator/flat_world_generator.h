#pragma once

#include "world_generator.h"

class FlatWorldGenerator : public WorldGenerator
{
	public:
		FlatWorldGenerator(BlockId grassId, BlockId stoneId);

		void Generate(Chunk& chunk) const override;

	private:
		BlockId grassId;
		BlockId stoneId;
};
