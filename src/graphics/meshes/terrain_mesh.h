#pragma once

#include "../../world/world.h"
#include "../../world/block_faces.h"
#include <DirectXMath.h>
#include <vector>
#include <map>
#include <future>
#include <chrono>

struct TerrainBatch
{
    BlockId block;
    unsigned int face, firstInstance, instanceCount;
};

struct TerrainMesh
{
    std::vector<DirectX::XMFLOAT3> instances;
    std::vector<TerrainBatch> batches;
    std::size_t blockCount = 0;
};

inline TerrainMesh BuildTerrainMesh(const World::Chunks& chunks)
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
    std::map<std::pair<BlockId, unsigned int>, std::vector<DirectX::XMFLOAT3>> groups;
    TerrainMesh mesh;
    for (const auto& [coordinates, chunk] : chunks)
        for (int y = 0; y < Chunk::Height; ++y)
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
                    for (unsigned int face = 0; face < 6; ++face)
                        if (faces & (1u << face))
                            groups[{block, face}].push_back({
                                worldX + 0.5f, y + 0.5f, worldZ + 0.5f});
                }
    for (const auto& [key, positions] : groups)
    {
        mesh.batches.push_back({.block = key.first, .face = key.second,
            .firstInstance = static_cast<unsigned int>(mesh.instances.size()),
            .instanceCount = static_cast<unsigned int>(positions.size())});
        mesh.instances.insert(mesh.instances.end(), positions.begin(), positions.end());
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
    const TerrainMesh& Get(const World& world)
    {
        if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto completed = pending.get();
            if (pendingWorld == &world && pendingRevision == world.GetRevision())
            {
                mesh = std::move(completed);
                cachedWorld = &world;
                cachedRevision = pendingRevision;
                ++generation;
            }
        }
        // The first mesh is built during startup; streamed replacements never block rendering.
        if (cachedWorld != &world)
        {
            mesh = BuildTerrainMesh(world);
            cachedWorld = &world;
            cachedRevision = world.GetRevision();
            ++generation;
        }
        else if (cachedRevision != world.GetRevision() && !pending.valid())
        {
            pendingWorld = &world;
            pendingRevision = world.GetRevision();
            // Own the snapshot: physics can stream chunks without racing the worker.
            pending = std::async(std::launch::async, [chunks = world.GetChunks()]
            {
                return BuildTerrainMesh(chunks);
            });
        }
        return mesh;
    }

    std::uint64_t GetGeneration() const { return generation; }

private:
    TerrainMesh mesh;
    const World* cachedWorld = nullptr;
    const World* pendingWorld = nullptr;
    std::uint64_t cachedRevision = 0, pendingRevision = 0, generation = 0;
    std::future<TerrainMesh> pending;
};
