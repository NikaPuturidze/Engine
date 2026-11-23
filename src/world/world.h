#pragma once

#include "generator/world_generator.h"
#include <map>
#include <memory>
#include <utility>

class World
{
public:
    using ChunkCoordinates = std::pair<int, int>;
    using Chunks = std::map<ChunkCoordinates, Chunk>;

    explicit World(std::unique_ptr<WorldGenerator> generator, int chunkRadius = 32  );

    void Update(float playerX, float playerZ);
    BlockId GetBlock(int worldX, int y, int worldZ) const;
    const Chunks& GetChunks() const { return chunks; }
    int GetChunkRadius() const { return chunkRadius; }
    std::uint64_t GetRevision() const { return revision; }

private:
    std::unique_ptr<WorldGenerator> generator;
    int chunkRadius;
    Chunks chunks;
    ChunkCoordinates center{};
    bool initialized = false;
    std::uint64_t revision = 0;
};
