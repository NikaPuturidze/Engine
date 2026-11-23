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
        for (int y = 0; y < chunk.GetStoredHeight(); ++y)
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
    loadOrder.emplace_back(0, 0);
    for (int ring = 1; ring <= chunkRadius; ++ring)
    {
        for (int x = -ring; x <= ring; ++x)
        {
            loadOrder.emplace_back(x, -ring);
            loadOrder.emplace_back(x, ring);
        }
        for (int z = -ring + 1; z < ring; ++z)
        {
            loadOrder.emplace_back(-ring, z);
            loadOrder.emplace_back(ring, z);
        }
    }
}

void World::Update(const float playerX, const float playerZ, const std::size_t chunkBudget)
{
    const ChunkCoordinates nextCenter{
        static_cast<int>(std::floor(playerX / Chunk::Width)),
        static_cast<int>(std::floor(playerZ / Chunk::Depth))
    };
    const auto [centerX, centerZ] = nextCenter;
    std::vector<ChunkCoordinates> changes;
    if (!initialized || center != nextCenter)
    {
        std::erase_if(chunks, [&](const auto& entry)
        {
            const auto [x, z] = entry.first;
            const bool remove = x < centerX - chunkRadius || x > centerX + chunkRadius ||
                                z < centerZ - chunkRadius || z > centerZ + chunkRadius;
            if (remove) changes.push_back(entry.first);
            return remove;
        });
        center = nextCenter;
        initialized = true;
        nextChunk = 0;
    }
    std::size_t generated = 0;
    // Bound queue scanning too, so moving through an already loaded world stays cheap.
    std::size_t scanned = 0;
    while (nextChunk < loadOrder.size() && generated < chunkBudget && scanned < 256)
    {
        const auto [offsetX, offsetZ] = loadOrder[nextChunk++];
        ++scanned;
        const ChunkCoordinates coordinates{centerX + offsetX, centerZ + offsetZ};
        if (chunks.contains(coordinates)) continue;

        Chunk chunk(coordinates.first, coordinates.second);
        generator->Generate(chunk);
        if (IsEmpty(chunk))
            throw std::runtime_error("Generated chunk is empty: " +
                std::to_string(coordinates.first) + ", " + std::to_string(coordinates.second));
        if (const auto saved = edits.find(coordinates); saved != edits.end())
            for (const auto& [position, block] : saved->second)
                chunk.SetBlock(position[0], position[1], position[2], block);
        chunks.emplace(coordinates, std::move(chunk));
        ++generated;
        changes.push_back(coordinates);
    }
    if (!changes.empty())
    {
        changedChunks = std::move(changes);
        ++revision;
    }
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

bool World::SetBlock(const int worldX, const int y, const int worldZ, const BlockId block)
{
    if (y < 0 || y >= Chunk::Height) return false;
    const int chunkX = FloorDivide(worldX, Chunk::Width);
    const int chunkZ = FloorDivide(worldZ, Chunk::Depth);
    const auto chunk = chunks.find({chunkX, chunkZ});
    if (chunk == chunks.end()) return false;
    const int x = worldX - chunkX * Chunk::Width;
    const int z = worldZ - chunkZ * Chunk::Depth;
    if (chunk->second.GetBlock(x, y, z) == block) return false;
    edits[{chunkX, chunkZ}][{x, y, z}] = block;
    chunk->second.SetBlock(x, y, z, block);
    changedChunks = {{chunkX, chunkZ}};
    ++revision;
    return true;
}
