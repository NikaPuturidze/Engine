#pragma once

#include "simulation/world/generator/world_generator.h"
#include <map>
#include <array>
#include <memory>
#include <utility>
#include <vector>
#include <optional>

class World
{
public:
    using ChunkCoordinates = std::pair<int, int>;
    using Chunks = std::map<ChunkCoordinates, Chunk>;
    using Edits = std::map<ChunkCoordinates, std::map<std::array<int, 3>, BlockId>>;

    explicit World(std::unique_ptr<WorldGenerator> generator, int chunkRadius = 32  );

    void Update(float playerX, float playerZ, std::size_t chunkBudget = 16);
    std::optional<BlockId> TryGetBlock(int worldX, int y, int worldZ) const;
    BlockId GetBlock(int worldX, int y, int worldZ) const;
    bool SetBlock(int worldX, int y, int worldZ, BlockId block);
    const Chunks& GetChunks() const { return chunks; }
    const std::vector<ChunkCoordinates>& GetChangedChunks() const { return changedChunks; }
    int GetChunkRadius() const { return chunkRadius; }
    std::uint64_t GetRevision() const { return revision; }
    const Edits& GetEdits() const { return edits; }
    // Install a validated snapshot before any chunks are generated.
    void RestoreEdits(const Edits& saved);

private:

    void BuildLoadOrder();
    void MoveStreamingCenter(const ChunkCoordinates& nextCenter, std::vector<ChunkCoordinates>& changes);
    void GenerateNearbyChunks(const std::size_t chunkBudget, std::vector<ChunkCoordinates>& changes);
    void GenerateChunk(const ChunkCoordinates& coordinates);
    void RestoreChunkEdits(Chunk& chunk, const ChunkCoordinates& coordinates) const;
    void PublishChanges(std::vector<ChunkCoordinates> changes);


    std::unique_ptr<WorldGenerator> generator;
    int chunkRadius;
    Chunks chunks;
    Edits edits;
    ChunkCoordinates center{};
    bool initialized = false;
    std::vector<ChunkCoordinates> loadOrder;
    std::vector<ChunkCoordinates> changedChunks;
    std::size_t nextChunk = 0;
    std::uint64_t revision = 0;
    std::uint64_t loadVersion = 0;
};
