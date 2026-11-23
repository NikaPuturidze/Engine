#pragma once

#include "block.h"

#include "../graphics/cube.h"
#include "../graphics/texture.h"
#include "../graphics/renderer.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

class BlockRegistry
{
	public:
		explicit BlockRegistry(Renderer& renderer);

		void LoadNamespace(const std::filesystem::path& directory, std::string_view namespaceName);
		BlockId GetBlockId(std::string_view resourceId) const;
		std::string_view GetBlockName(BlockId id) const;
		std::shared_ptr<const BlockDefinition> GetBlockDefinition(BlockId id) const;
		Block CreateBlock(BlockId id, BlockPosition position) const;

	private:
		Renderer& renderer;
		BlockId nextBlockId = 1;
		TextureId nextTextureId = 1;
		std::unordered_map<std::string, BlockId> blockIds;
		std::unordered_map<std::string, TextureId> textureIds;
		std::unordered_map<BlockId, std::shared_ptr<const BlockDefinition>> definitions;
};
