#pragma once

#include "../../graphics/cube.h"
#include <array>
#include <cassert>
#include <cstddef>

class Chunk
{
	public:
		static constexpr int Width = 16;
		static constexpr int Depth = 16;
		static constexpr int Height = 3;

		Chunk(const int chunkX, const int chunkZ)
			: chunkX(chunkX), chunkZ(chunkZ)
		{
		}

		int GetChunkX() const { return chunkX; }
		int GetChunkZ() const { return chunkZ; }

		BlockId GetBlock(const int x, const int y, const int z) const
		{
			return blocks[GetIndex(x, y, z)];
		}

		void SetBlock(const int x, const int y, const int z, const BlockId block)
		{
			blocks[GetIndex(x, y, z)] = block;
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
		std::array<BlockId, Width * Height * Depth> blocks{};
};