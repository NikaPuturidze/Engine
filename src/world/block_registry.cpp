#include "block_registry.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
	std::vector<std::string> Split(const std::string& text, char separator)
	{
		std::vector<std::string> result;
		std::istringstream stream(text);
		std::string value;
		while (std::getline(stream, value, separator)) result.push_back(value);
		return result;
	}
}

BlockRegistry::BlockRegistry(Renderer& renderer) : renderer(renderer) {}

void BlockRegistry::LoadNamespace(const std::filesystem::path& directory, std::string_view namespaceName)
{
	std::ifstream manifest(directory / "engine/blocks.tsv");
	std::string line;
	if (!std::getline(manifest, line) || line != "engine-blocks-v3")
		throw std::runtime_error("Missing or invalid block manifest: " + (directory / "engine/blocks.tsv").string());
	auto loadTexture = [&](const std::string& reference)
	{
		const std::string key = std::string(namespaceName) + ':' + reference;
		if (const auto found = textureIds.find(key); found != textureIds.end()) return found->second;
		const TextureId id = nextTextureId++;
		renderer.RegisterTexture(Texture::LoadPng(id, directory / "textures" / (reference + ".png"), true));
		textureIds.emplace(key, id);
		return id;
	};
	std::vector<TextureId> cracks;
	for (int stage = 0; stage < 10; ++stage)
		cracks.push_back(loadTexture("block/destroy_stage_" + std::to_string(stage)));
	for (const auto* air : {"air", "cave_air", "void_air"})
		blockIds.emplace(std::string(namespaceName) + ":block/" + air, 0);
	while (std::getline(manifest, line))
	{
		if (line.empty()) continue;
		const auto fields = Split(line, '\t');
		if (fields.size() != 19) throw std::runtime_error("Invalid block manifest row: " + line);
		const auto resourceId = std::string(namespaceName) + ":block/" + fields[0];
		if (blockIds.contains(resourceId)) throw std::runtime_error("Duplicate block: " + resourceId);
		auto definition = std::make_shared<BlockDefinition>();
		definition->id = nextBlockId++;
		for (std::size_t face = 0; face < 6; ++face)
			definition->faceTextures[face] = loadTexture(fields[1 + face]);
		definition->sideTexture = definition->faceTextures[0];
		definition->topTexture = definition->faceTextures[4];
		definition->bottomTexture = definition->faceTextures[5];
		definition->maxDurability = std::stof(fields[7]);
		definition->isDestructible = fields[8] == "1";
		definition->isOpenable = fields[9] == "1";
		definition->requiresToolForDrops = fields[17] == "1";
		definition->crackingTextures = cracks;
		auto soundPaths = [&](std::size_t index)
		{
			auto paths = Split(fields[index], ',');
			for (auto& path : paths) path = (directory / path).string();
			return paths;
		};
		definition->movementSounds = soundPaths(13);
		definition->breakingSounds = soundPaths(14);
		definition->placingSounds = soundPaths(15);
		definition->hittingSounds = soundPaths(16);
		definition->fallingSounds = soundPaths(18);
		renderer.BindTextures(definition->id, definition->sideTexture, definition->topTexture,
			definition->bottomTexture, std::stof(fields[10]), std::stof(fields[11]), std::stof(fields[12]));
		renderer.BindFaceTextures(definition->id, definition->faceTextures);
		if (fields[0] == "grass_block")
			renderer.BindSideOverlay(definition->id, loadTexture("block/grass_block_side_overlay"));
		blockIds.emplace(resourceId, definition->id);
		definitions.emplace(definition->id, std::move(definition));
	}
}

std::shared_ptr<const BlockDefinition> BlockRegistry::GetBlockDefinition(BlockId id) const
{
	const auto found = definitions.find(id);
	if (found == definitions.end()) throw std::runtime_error("Unknown block ID: " + std::to_string(id));
	return found->second;
}

Block BlockRegistry::CreateBlock(const BlockId id, const BlockPosition position) const
{
	return Block(GetBlockDefinition(id), position);
}

BlockId BlockRegistry::GetBlockId(const std::string_view resourceId) const
{
	const auto found = blockIds.find(std::string(resourceId));
	if (found == blockIds.end()) throw std::runtime_error("Unknown block resource: " + std::string(resourceId));
	return found->second;
}

std::string_view BlockRegistry::GetBlockName(const BlockId id) const
{
	for (const auto& [name, blockId] : blockIds)
		if (blockId == id) return name;
	return "unknown";
}
