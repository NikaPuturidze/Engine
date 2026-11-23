#include "block_registry.h"
#include "../graphics/renderer.h"
#include <algorithm>
#include <fstream>
#include <optional>
#include <regex>
#include <stdexcept>
#include <vector>

BlockRegistry::BlockRegistry(Renderer& renderer) : renderer(renderer)
{
}

void BlockRegistry::LoadNamespace(const std::filesystem::path& directory, const std::string_view namespaceName)
{
	const std::filesystem::path blockstates = directory / "blockstates";

	std::vector<std::filesystem::path> stateFiles;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(blockstates))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".json")
			stateFiles.push_back(entry.path());
	}

	std::ranges::sort(stateFiles);

	for (const auto& statePath : stateFiles)
	{
		const std::filesystem::path relative = std::filesystem::relative(statePath, blockstates);

		std::filesystem::path blockPath = relative;
		blockPath.replace_extension();

		const std::string blockResourceId = std::string(namespaceName) + ":block/" + blockPath.generic_string();
		const BlockId blockId = nextBlockId++;

		const std::filesystem::path modelPath = ResolveResourcePath(
			directory,
			"models",
			ReadModelReference(statePath),
			".json"
		);
		const BlockTextureReferences textureReferences = ReadTextureReferences(modelPath);
		auto loadTexture = [&](const std::string& textureReference)
		{
			auto texture = textureIds.find(textureReference);
			if (texture == textureIds.end())
			{
				const TextureId textureId = nextTextureId++;
				const std::filesystem::path texturePath = ResolveResourcePath(
					directory,
					"textures",
					textureReference,
					".png"
				);
				renderer.RegisterTexture(Texture::LoadPng(textureId, texturePath));
				texture = textureIds.emplace(textureReference, textureId).first;
			}

			return texture->second;
		};

		renderer.BindTextures(
			blockId,
			loadTexture(textureReferences.side),
			loadTexture(textureReferences.top),
			loadTexture(textureReferences.bottom),
			textureReferences.topTintRed,
			textureReferences.topTintGreen,
			textureReferences.topTintBlue
		);
		blockIds.emplace(blockResourceId, blockId);
	}
}

BlockId BlockRegistry::GetBlockId(const std::string_view resourceId) const
{
	const auto block = blockIds.find(std::string(resourceId));
	if (block == blockIds.end())
		throw std::runtime_error("Unknown block resource: " + std::string(resourceId));

	return block->second;
}

std::string BlockRegistry::ReadTextFile(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file)
		throw std::runtime_error("Failed to open resource file: " + path.string());

	return {
		std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()
	};
}

std::string BlockRegistry::ReadModelReference(const std::filesystem::path& statePath)
{
	const std::regex pattern(R"json("model"\s*:\s*"([^"]+)")json");
	std::smatch match;
	const std::string state = ReadTextFile(statePath);

	if (!std::regex_search(state, match, pattern))
		throw std::runtime_error("Block state has no model: " + statePath.string());

	return match[1].str();
}

BlockRegistry::BlockTextureReferences BlockRegistry::ReadTextureReferences(
	const std::filesystem::path& modelPath
)
{
	const std::string model = ReadTextFile(modelPath);
	auto findTexture = [&](const std::string& name) -> std::optional<std::string>
	{
		const std::regex pattern("\\\"" + name + "\\\"\\s*:\\s*\\\"([^\\\"]+)\\\"");

		if (std::smatch match; std::regex_search(model, match, pattern))
			return match[1].str();

		return std::nullopt;
	};

	if (const auto all = findTexture("all"))
		return {.side = *all, .top = *all, .bottom = *all};

	const auto side = findTexture("side");
	const auto top = findTexture("top");
	const auto bottom = findTexture("bottom");

	if (!side || !top || !bottom)
		throw std::runtime_error("Cube model needs all or side, top, and bottom textures: " + modelPath.string());

	BlockTextureReferences textures = {
		.side = *side,
		.top = *top,
		.bottom = *bottom,
	};

	const std::regex tintPattern(
		R"json("top_tint"\s*:\s*\[\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*\])json"
	);

	if (std::smatch tintMatch; std::regex_search(model, tintMatch, tintPattern))
	{
		textures.topTintRed = std::stof(tintMatch[1].str());
		textures.topTintGreen = std::stof(tintMatch[2].str());
		textures.topTintBlue = std::stof(tintMatch[3].str());
	}

	return textures;
}

std::filesystem::path BlockRegistry::ResolveResourcePath(
	const std::filesystem::path& directory,
	const std::string_view resourceType,
	const std::string_view resourceId,
	const std::string_view extension
)
{
	const std::size_t separator = resourceId.find(':');
	if (separator == std::string_view::npos)
		throw std::runtime_error("Resource ID needs a namespace: " + std::string(resourceId));

	const std::string_view path = resourceId.substr(separator + 1);
	return directory / std::string(resourceType) / (std::string(path) + std::string(extension));
}
