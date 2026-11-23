#pragma once

#include "generator/world_generator.h"
#include <map>
#include <array>
#include <memory>
#include <utility>
#include <vector>

class World
{
public:
    using ChunkCoordinates = std::pair<int, int>;
    using Chunks = std::map<ChunkCoordinates, Chunk>;

    explicit World(std::unique_ptr<WorldGenerator> generator, int chunkRadius = 32  );

    void Update(float playerX, float playerZ, std::size_t chunkBudget = 16);
    BlockId GetBlock(int worldX, int y, int worldZ) const;
    bool SetBlock(int worldX, int y, int worldZ, BlockId block);
    const Chunks& GetChunks() const { return chunks; }
    const std::vector<ChunkCoordinates>& GetChangedChunks() const { return changedChunks; }
    int GetChunkRadius() const { return chunkRadius; }
    std::uint64_t GetRevision() const { return revision; }

private:
    std::unique_ptr<WorldGenerator> generator;
    int chunkRadius;
    Chunks chunks;
    std::map<ChunkCoordinates, std::map<std::array<int, 3>, BlockId>> edits;
    ChunkCoordinates center{};
    bool initialized = false;
    std::vector<ChunkCoordinates> loadOrder;
    std::vector<ChunkCoordinates> changedChunks;
    std::size_t nextChunk = 0;
    std::uint64_t revision = 0;
};
