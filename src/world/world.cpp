#include "world.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace
{
    int FloorDivide(int value, int divisor)
    {
        const int quotient = value / divisor;
        return quotient - (value % divisor < 0 ? 1 : 0);
    }

    bool IsEmpty(const Chunk& chunk)
    {
        for (int y = 0; y < Chunk::Height; ++y)
            for (int z = 0; z < Chunk::Depth; ++z)
                for (int x = 0; x < Chunk::Width; ++x)
                    if (chunk.GetBlock(x, y, z) != 0) return false;
        return true;
    }
}

World::World(std::unique_ptr<WorldGenerator> generator, const int chunkRadius)
    : generator(std::move(generator)), chunkRadius(chunkRadius)
{
    if (!this->generator) throw std::invalid_argument("World needs a generator");
    if (chunkRadius < 0) throw std::invalid_argument("Chunk radius cannot be negative");
}

void World::Update(const float playerX, const float playerZ)
{
    const ChunkCoordinates nextCenter{
        static_cast<int>(std::floor(playerX / Chunk::Width)),
        static_cast<int>(std::floor(playerZ / Chunk::Depth))
    };
    if (initialized && center == nextCenter) return;

    const auto [centerX, centerZ] = nextCenter;
    std::erase_if(chunks, [&](const auto& entry)
    {
        const auto [x, z] = entry.first;
        return x < centerX - chunkRadius || x > centerX + chunkRadius ||
               z < centerZ - chunkRadius || z > centerZ + chunkRadius;
    });

    for (int z = centerZ - chunkRadius; z <= centerZ + chunkRadius; ++z)
    {
        for (int x = centerX - chunkRadius; x <= centerX + chunkRadius; ++x)
        {
            const ChunkCoordinates coordinates{x, z};
            if (chunks.contains(coordinates)) continue;

            Chunk chunk(x, z);
            generator->Generate(chunk);
            if (IsEmpty(chunk))
                throw std::runtime_error("Generated chunk is empty: " +
                    std::to_string(x) + ", " + std::to_string(z));
            chunks.emplace(coordinates, std::move(chunk));
        }
    }
    center = nextCenter;
    initialized = true;
    ++revision;
}

BlockId World::GetBlock(const int worldX, const int y, const int worldZ) const
{
    if (y < 0 || y >= Chunk::Height) return 0;
    const int chunkX = FloorDivide(worldX, Chunk::Width);
    const int chunkZ = FloorDivide(worldZ, Chunk::Depth);
    const auto chunk = chunks.find({chunkX, chunkZ});
    if (chunk == chunks.end()) return 0;
    return chunk->second.GetBlock(
        worldX - chunkX * Chunk::Width, y,
        worldZ - chunkZ * Chunk::Depth);
}
