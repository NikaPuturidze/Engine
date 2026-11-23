#include "flat_world_generator.h"

FlatWorldGenerator::FlatWorldGenerator(
	const BlockId grassId, const BlockId stoneId) : grassId(grassId), stoneId(stoneId)
{
}

void FlatWorldGenerator::Generate(Chunk& chunk) const
{
	for (int y = 0; y < Chunk::Height; ++y)
	{
		const BlockId block = (y == 2) ? grassId : stoneId;

		for (int z = 0; z < Chunk::Depth; ++z)
		{
			for (int x = 0; x < Chunk::Width; ++x)
			{
				chunk.SetBlock(x, y, z, block);
			}
		}
	}
}
