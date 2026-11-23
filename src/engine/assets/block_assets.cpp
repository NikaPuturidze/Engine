#include "engine/assets/block_assets.h"
#include "engine/graphics/renderer.h"
#include <fstream>
#include <sstream>

namespace
{
 std::vector<std::string> Split(const std::string& text, const char separator)
{
    std::vector<std::string> fields;
    std::istringstream stream(text);
    std::string field;
    while (std::getline(stream, field, separator)) fields.push_back(field);
    return fields;
}

 BlockDefinition ReadDefinition(const std::vector<std::string>& fields)
{
    BlockDefinition definition;
    definition.maxDurability = std::stof(fields[7]);
    definition.isDestructible = fields[8] == "1";
    definition.isOpenable = fields[9] == "1";
    definition.requiresToolForDrops = fields[17] == "1";
    definition.movementSounds = Split(fields[13], ',');
    definition.breakingSounds = Split(fields[14], ',');
    definition.placingSounds = Split(fields[15], ',');
    definition.hittingSounds = Split(fields[16], ',');
    definition.fallingSounds = Split(fields[18], ',');
    return definition;
}
}

void BlockAssets::Load(const std::filesystem::path& directory, BlockRegistry& blocks)
{
    std::ifstream manifest(directory / "engine/blocks.tsv");
    std::string version;
    if (!std::getline(manifest, version) || version != "engine-blocks-v3")
        throw std::runtime_error("Missing or invalid block manifest: " + directory.string());
    textureIds.clear();
    LoadCrackTextures(directory);
    LoadBlockRows(manifest, directory, blocks);
}

void BlockAssets::LoadCrackTextures(const std::filesystem::path& directory)
{
    for (int stage = 0; stage < 10; ++stage)
        cracks.push_back(FindOrAddTexture(directory, "block/destroy_stage_" + std::to_string(stage)));
}

void BlockAssets::LoadBlockRows(std::istream& manifest, const std::filesystem::path& directory, BlockRegistry& blocks)
{
    std::string row;
    while (std::getline(manifest, row))
    {
        if (row.empty()) continue;
        const auto fields = Split(row, '\t');
        if (fields.size() != 19) throw std::runtime_error("Invalid block manifest row: " + row);
        const auto block = blocks.Register("minecraft:block/" + fields[0], ReadDefinition(fields));
        materials.push_back(ReadMaterial(directory, fields, block));
    }
}

TextureId BlockAssets::FindOrAddTexture(const std::filesystem::path& directory, const std::string& reference)
{
    const auto found = textureIds.find(reference);
    if (found != textureIds.end()) return found->second;
    const TextureId id = textures.size() + 1;
    textures.push_back(
        {
            .id = id,
            .path = directory / "textures" / (reference + ".png")
        }
    );
    textureIds.emplace(reference, id);
    return id;
}

BlockMaterial BlockAssets::ReadMaterial(const std::filesystem::path& directory,
    const std::vector<std::string>& fields, const BlockId block)
{
    BlockMaterial material;
    material.block = block;
    for (std::size_t face = 0; face < 6; ++face)
        material.faces[face] = FindOrAddTexture(directory, fields[1 + face]);
    material.red = std::stof(fields[10]);
    material.green = std::stof(fields[11]);
    material.blue = std::stof(fields[12]);
    if (fields[0] == "grass_block")
        material.sideOverlay = FindOrAddTexture(directory, "block/grass_block_side_overlay");
    return material;
}

void BlockAssets::Upload(Renderer& renderer) const
{
    UploadTextures(renderer);
    BindMaterials(renderer);
}

void BlockAssets::UploadTextures(Renderer& renderer) const
{
    for (const auto& source : textures)
        renderer.RegisterTexture(Texture::LoadPng(source.id, source.path, true));
}

void BlockAssets::BindMaterials(Renderer& renderer) const
{
    for (const auto& material : materials)
    {
        renderer.BindTextures(material.block, material.faces[0], material.faces[4], material.faces[5],
            material.red, material.green, material.blue);
        renderer.BindFaceTextures(material.block, material.faces);
        if (material.sideOverlay) renderer.BindSideOverlay(material.block, material.sideOverlay);
    }
}

std::optional<TextureId> BlockAssets::CrackTexture(const float progress) const
{
    if (progress <= 0 || progress >= 1 || cracks.empty()) return std::nullopt;
    return cracks[std::min(static_cast<std::size_t>(progress * cracks.size()), cracks.size() - 1)];
}
