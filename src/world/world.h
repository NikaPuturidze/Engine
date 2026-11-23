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

    explicit World(std::unique_ptr<WorldGenerator> generator, int chunkRadius = 2);

    void Update(float playerX, float playerZ);
    BlockId GetBlock(int worldX, int y, int worldZ) const;
    const Chunks& GetChunks() const { return chunks; }

private:
    std::unique_ptr<WorldGenerator> generator;
    int chunkRadius;
    Chunks chunks;
    ChunkCoordinates center{};
    bool initialized = false;
};
