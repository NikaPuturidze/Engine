#pragma once

#include "../../graphics/cube.h"
#include <vector>
#include <cassert>
#include <cstddef>

class Chunk
{
	public:
		static constexpr int Width = 16;
		static constexpr int Depth = 16;
		static constexpr int Height = 256;

		Chunk(const int chunkX, const int chunkZ)
			: chunkX(chunkX), chunkZ(chunkZ)
		{
		}

		int GetChunkX() const { return chunkX; }
		int GetChunkZ() const { return chunkZ; }
		int GetStoredHeight() const { return static_cast<int>(blocks.size() / (Width * Depth)); }

		BlockId GetBlock(const int x, const int y, const int z) const
		{
			const auto index = GetIndex(x, y, z);
			return index < blocks.size() ? blocks[index] : 0;
		}

		void SetBlock(const int x, const int y, const int z, const BlockId block)
		{
			const auto index = GetIndex(x, y, z);
			if (index >= blocks.size())
			{
				if (block == 0) return;
				blocks.resize(static_cast<std::size_t>(y + 1) * Width * Depth);
			}
			blocks[index] = block;
		}

	private:
		static std::size_t GetIndex(const int x, const int y, const int z)
		{
			assert(x >= 0 && x < Width);
			assert(y >= 0 && y < Height);
			assert(z >= 0 && z < Depth);

			return static_cast<std::size_t>(
				x + Width * (z + Depth * y)
			);
		}

		int chunkX;
		int chunkZ;
		std::vector<BlockId> blocks;
};
