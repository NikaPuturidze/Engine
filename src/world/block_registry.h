#pragma once

#include "../graphics/cube.h"
#include "../graphics/texture.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

class Renderer;

class BlockRegistry
{
public:
	explicit BlockRegistry(Renderer& renderer);

	void LoadNamespace(const std::filesystem::path& directory, std::string_view namespaceName);
	BlockId GetBlockId(std::string_view resourceId) const;

private:
	std::string ReadTextFile(const std::filesystem::path& path) const;
	std::string ReadModelReference(const std::filesystem::path& statePath) const;
	struct BlockTextureReferences
	{
		std::string side;
		std::string top;
		std::string bottom;
		float topTintRed = 1.0f;
		float topTintGreen = 1.0f;
		float topTintBlue = 1.0f;
	};

	BlockTextureReferences ReadTextureReferences(const std::filesystem::path& modelPath) const;
	std::filesystem::path ResolveResourcePath(
		const std::filesystem::path& directory,
		std::string_view resourceType,
		std::string_view resourceId,
		std::string_view extension
	) const;

	Renderer& renderer;
	BlockId nextBlockId = 1;
	TextureId nextTextureId = 1;
	std::unordered_map<std::string, BlockId> blockIds;
	std::unordered_map<std::string, TextureId> textureIds;
};
