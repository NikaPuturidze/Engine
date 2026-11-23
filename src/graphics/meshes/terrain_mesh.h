#pragma once

#include "../../world/world.h"
#include "../../world/block_faces.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <queue>
#include <set>
#include <optional>

struct TerrainBatch
{
    BlockId block;
    unsigned int face, firstInstance, instanceCount;
    std::size_t chunkIndex;
};

struct TerrainChunk
{
    DirectX::BoundingBox bounds;
    std::size_t blockCount = 0;
};

struct TerrainMesh
{
    std::vector<DirectX::XMFLOAT3> instances;
    std::vector<TerrainBatch> batches;
    std::vector<TerrainChunk> chunks;
    std::size_t blockCount = 0;
};

inline TerrainMesh BuildTerrainMesh(const World::Chunks& chunks,
    const World::ChunkCoordinates* selectedChunk = nullptr)
{
    const auto isSolid = [&](int x, int y, int z)
    {
        if (y < 0 || y >= Chunk::Height) return false;
        const int chunkX = x / Chunk::Width - (x % Chunk::Width < 0 ? 1 : 0);
        const int chunkZ = z / Chunk::Depth - (z % Chunk::Depth < 0 ? 1 : 0);
        const auto chunk = chunks.find({chunkX, chunkZ});
        return chunk != chunks.end() && chunk->second.GetBlock(
            x - chunkX * Chunk::Width, y, z - chunkZ * Chunk::Depth) != 0;
    };
    TerrainMesh mesh;
    const auto first = selectedChunk ? chunks.find(*selectedChunk) : chunks.begin();
    const auto last = selectedChunk && first != chunks.end() ? std::next(first) : chunks.end();
    for (auto entry = first; entry != last; ++entry)
    {
        const auto& [coordinates, chunk] = *entry;
        std::map<std::pair<BlockId, unsigned int>, std::vector<DirectX::XMFLOAT3>> groups;
        TerrainChunk terrainChunk{
            .bounds = DirectX::BoundingBox{
                {coordinates.first * Chunk::Width + Chunk::Width * 0.5f,
                 chunk.GetStoredHeight() * 0.5f,
                 coordinates.second * Chunk::Depth + Chunk::Depth * 0.5f},
                {Chunk::Width * 0.5f, chunk.GetStoredHeight() * 0.5f, Chunk::Depth * 0.5f}}
        };
        for (int y = 0; y < chunk.GetStoredHeight(); ++y)
            for (int z = 0; z < Chunk::Depth; ++z)
                for (int x = 0; x < Chunk::Width; ++x)
                {
                    const auto block = chunk.GetBlock(x, y, z);
                    if (block == 0) continue;
                    const int worldX = coordinates.first * Chunk::Width + x;
                    const int worldZ = coordinates.second * Chunk::Depth + z;
                    const auto faces = GetVisibleBlockFaces(worldX, y, worldZ, isSolid);
                    if (faces == 0) continue;
                    ++mesh.blockCount;
                    ++terrainChunk.blockCount;
                    for (unsigned int face = 0; face < 6; ++face)
                        if (faces & (1u << face))
                            groups[{block, face}].push_back({
                                worldX + 0.5f, y + 0.5f, worldZ + 0.5f});
                }
        for (const auto& [key, positions] : groups)
        {
            mesh.batches.push_back({.block = key.first, .face = key.second,
                .firstInstance = static_cast<unsigned int>(mesh.instances.size()),
                .instanceCount = static_cast<unsigned int>(positions.size()),
                .chunkIndex = mesh.chunks.size()});
            mesh.instances.insert(mesh.instances.end(), positions.begin(), positions.end());
        }
        mesh.chunks.push_back(terrainChunk);
    }
    return mesh;
}

inline TerrainMesh BuildTerrainMesh(const World& world)
{
    return BuildTerrainMesh(world.GetChunks());
}

class TerrainMeshCache
{
public:
    struct Entry
    {
        TerrainMesh mesh;
        unsigned int neighbors = 0;
        std::uint64_t generation = 0;
    };
    using Entries = std::map<World::ChunkCoordinates, Entry>;

    const Entries& Get(const World& world, const DirectX::XMFLOAT3& position,
        std::size_t chunkBudget = 8)
    {
        const auto& chunks = world.GetChunks();
        if (cachedWorld != &world)
        {
            entries.clear();
            pending = {};
            queued.clear();
            cachedWorld = &world;
            cachedRevision = world.GetRevision();
            scanning = true;
            scanPosition.reset();
        }
        const auto enqueue = [&](const World::ChunkCoordinates& coordinates)
        {
            if (!chunks.contains(coordinates))
            {
                entries.erase(coordinates);
                return;
            }
            const auto existing = entries.find(coordinates);
            if (existing != entries.end() && existing->second.neighbors == NeighborMask(chunks, coordinates))
                return;
            if (!queued.insert(coordinates).second) return;
            const float x = (coordinates.first + 0.5f) * Chunk::Width - position.x;
            const float z = (coordinates.second + 0.5f) * Chunk::Depth - position.z;
            pending.push({x * x + z * z, coordinates});
        };
        if (cachedRevision != world.GetRevision())
        {
            if (cachedRevision + 1 != world.GetRevision())
            {
                // A caller skipped updates: reconcile in bounded scan steps.
                scanning = true;
                scanPosition.reset();
                entries.clear();
            }
            for (const auto& coordinates : world.GetChangedChunks())
            {
                const auto [x, z] = coordinates;
                // Contents can change without changing the neighbor mask.
                entries.erase(coordinates);
                entries.erase({x - 1, z});
                entries.erase({x + 1, z});
                entries.erase({x, z - 1});
                entries.erase({x, z + 1});
                enqueue(coordinates);
                enqueue({x - 1, z});
                enqueue({x + 1, z});
                enqueue({x, z - 1});
                enqueue({x, z + 1});
            }
            cachedRevision = world.GetRevision();
        }
        if (scanning)
        {
            auto next = scanPosition ? chunks.upper_bound(*scanPosition) : chunks.begin();
            for (unsigned int i = 0; next != chunks.end() && i < 128; ++i, ++next)
            {
                enqueue(next->first);
                scanPosition = next->first;
            }
            scanning = next != chunks.end();
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
        std::size_t built = 0, examined = 0;
        while (!pending.empty() && built < chunkBudget && examined++ < 128)
        {
            const auto coordinates = pending.top().second;
            pending.pop();
            queued.erase(coordinates);
            if (!chunks.contains(coordinates)) continue;
            const auto neighbors = NeighborMask(chunks, coordinates);
            const auto existing = entries.find(coordinates);
            if (existing != entries.end() && existing->second.neighbors == neighbors) continue;
            entries[coordinates] = {BuildTerrainMesh(chunks, &coordinates), neighbors, ++generation};
            ++built;
            if (std::chrono::steady_clock::now() >= deadline) break;
        }
        return entries;
    }

private:
    static unsigned int NeighborMask(const World::Chunks& chunks, const World::ChunkCoordinates& coordinates)
    {
        const auto [x, z] = coordinates;
        return (chunks.contains({x - 1, z}) ? 1u : 0u) |
               (chunks.contains({x + 1, z}) ? 2u : 0u) |
               (chunks.contains({x, z - 1}) ? 4u : 0u) |
               (chunks.contains({x, z + 1}) ? 8u : 0u);
    }

    using PendingChunk = std::pair<float, World::ChunkCoordinates>;
    std::priority_queue<PendingChunk, std::vector<PendingChunk>, std::greater<PendingChunk>> pending;
    std::set<World::ChunkCoordinates> queued;
    std::optional<World::ChunkCoordinates> scanPosition;
    bool scanning = false;
    std::uint64_t cachedRevision = 0;
    Entries entries;
    const World* cachedWorld = nullptr;
    std::uint64_t generation = 0;
};
