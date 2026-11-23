#include "simulation/world/world.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace
{
    int FloorDivide(const int value, const int divisor)
    {
        const int quotient = value / divisor;
        return quotient - (value % divisor < 0 ? 1 : 0);
    }

}

World::World(std::unique_ptr<WorldGenerator> generator, const int chunkRadius)
    : generator(std::move(generator)), chunkRadius(chunkRadius)
{
    if (!this->generator) throw std::invalid_argument("World needs a generator");
    if (chunkRadius < 0) throw std::invalid_argument("Chunk radius cannot be negative");
    BuildLoadOrder();
}

void World::RestoreEdits(const Edits& saved)
{
    if (initialized) throw std::logic_error("Restore edits before generating terrain");
    edits = saved;
}

void World::BuildLoadOrder()
{
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
    std::vector<ChunkCoordinates> changes;
    MoveStreamingCenter(nextCenter, changes);
    GenerateNearbyChunks(chunkBudget, changes);
    PublishChanges(std::move(changes));
}

void World::MoveStreamingCenter(const ChunkCoordinates& nextCenter, std::vector<ChunkCoordinates>& changes)
{
    if (initialized && center == nextCenter) return;
    const auto [centerX, centerZ] = nextCenter;
    std::erase_if(chunks, [&](const auto& entry)
    {
        const auto [x, z] = entry.first;
        const bool outside = x < centerX - chunkRadius || x > centerX + chunkRadius ||
                             z < centerZ - chunkRadius || z > centerZ + chunkRadius;
        if (outside) changes.push_back(entry.first);
        return outside;
    });
    center = nextCenter;
    initialized = true;
    nextChunk = 0;
}

void World::GenerateNearbyChunks(const std::size_t chunkBudget, std::vector<ChunkCoordinates>& changes)
{
    std::size_t generated = 0;
    std::size_t scanned = 0;

    while (nextChunk < loadOrder.size() && generated < chunkBudget && scanned < 256)
    {
        const auto [offsetX, offsetZ] = loadOrder[nextChunk++];
        ++scanned;
        const ChunkCoordinates coordinates{center.first + offsetX, center.second + offsetZ};
        if (chunks.contains(coordinates)) continue;
        GenerateChunk(coordinates);
        changes.push_back(coordinates);
        ++generated;
    }
}

void World::GenerateChunk(const ChunkCoordinates& coordinates)
{
    Chunk chunk(coordinates.first, coordinates.second);
    generator->Generate(chunk);
    RestoreChunkEdits(chunk, coordinates);
    chunk.SetLoadVersion(++loadVersion);
    chunks.emplace(coordinates, std::move(chunk));
}

void World::RestoreChunkEdits(Chunk& chunk, const ChunkCoordinates& coordinates) const
{
    const auto saved = edits.find(coordinates);
    if (saved == edits.end()) return;
    for (const auto& [position, block] : saved->second)
        chunk.SetBlock(position[0], position[1], position[2], block);
}

void World::PublishChanges(std::vector<ChunkCoordinates> changes)
{
    if (changes.empty()) return;
    changedChunks = std::move(changes);
    ++revision;
}

BlockId World::GetBlock(const int worldX, const int y, const int worldZ) const
{
    return TryGetBlock(worldX, y, worldZ).value_or(Air);
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

std::optional<BlockId> World::TryGetBlock(const int worldX, const int y, const int worldZ) const
{
    if (y<0 || y>=Chunk::Height) return Air;
    const int cx=FloorDivide(worldX,Chunk::Width), cz=FloorDivide(worldZ,Chunk::Depth);
    const auto found=chunks.find({cx,cz});
    if (found==chunks.end()) return std::nullopt;
    return found->second.GetBlock(worldX-cx*Chunk::Width,y,worldZ-cz*Chunk::Depth);
}
