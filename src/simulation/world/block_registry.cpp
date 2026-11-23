#include "simulation/world/block_registry.h"
BlockRegistry::BlockRegistry()
{
    for (const char* name : {"air", "cave_air", "void_air"})
        blockIds.emplace(std::string("minecraft:block/")+name, Air);
    names.emplace(Air, "minecraft:block/air");
}
BlockId BlockRegistry::Register(std::string name, BlockDefinition definition)
{
    if (blockIds.contains(name)) throw std::invalid_argument("Duplicate block: "+name);
    definition.id=nextBlockId++;
    blockIds.emplace(name, definition.id);
    names.emplace(definition.id, std::move(name));
    const auto id=definition.id;
    definitions.emplace(id, std::make_shared<const BlockDefinition>(std::move(definition)));
    return id;
}
BlockId BlockRegistry::GetBlockId(const std::string_view name) const
{
    auto found=blockIds.find(std::string(name));
    if (found==blockIds.end()) throw std::runtime_error("Unknown block resource: "+std::string(name));
    return found->second;
}
std::string_view BlockRegistry::GetBlockName(const BlockId id) const
{
    auto found=names.find(id);
    return found==names.end() ? "unknown" : std::string_view(found->second);
}
std::shared_ptr<const BlockDefinition> BlockRegistry::GetBlockDefinition(const BlockId id) const
{
    auto found=definitions.find(id);
    if (found==definitions.end()) throw std::runtime_error("Unknown block ID: "+std::to_string(id));
    return found->second;
}
