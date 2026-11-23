#pragma once
#include "simulation/world/block_definition.h"
#include <string>
#include <string_view>
#include <unordered_map>
// CPU-only gameplay catalog. No file I/O or graphics dependencies.
class BlockRegistry
{
public:
    BlockRegistry();
    BlockId Register(std::string name, BlockDefinition definition);
    BlockId GetBlockId(std::string_view name) const;
    std::string_view GetBlockName(BlockId id) const;
    std::shared_ptr<const BlockDefinition> GetBlockDefinition(BlockId id) const;
    bool Contains(const BlockId id) const { return id==Air || definitions.contains(id); }
private:
    BlockId nextBlockId=1;
    std::unordered_map<std::string, BlockId> blockIds;
    std::unordered_map<BlockId, std::string> names;
    std::unordered_map<BlockId, std::shared_ptr<const BlockDefinition>> definitions;
};
