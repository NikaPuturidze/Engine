#pragma once
#include "simulation/world/block_registry.h"
#include "engine/graphics/texture.h"
#include <array>
#include <filesystem>
#include <vector>
#include <istream>
#include <unordered_map>
class Renderer;
struct TextureSource { TextureId id; std::filesystem::path path; };
struct BlockMaterial
{
    BlockId block;
    std::array<TextureId,6> faces;
    float red=1,green=1,blue=1;
    TextureId sideOverlay=0;
};
// Asset adapter joins CPU gameplay definitions and independent visual descriptions.
class BlockAssets
{
public:
    void Load(const std::filesystem::path& directory, BlockRegistry& blocks);
    void Upload(Renderer& renderer) const;
    std::optional<TextureId> CrackTexture(float progress) const;
private:

    std::unordered_map<std::string, TextureId> textureIds;
    void LoadCrackTextures(const std::filesystem::path& directory);
    void LoadBlockRows(std::istream& manifest, const std::filesystem::path& directory, BlockRegistry& blocks);
    TextureId FindOrAddTexture(const std::filesystem::path& directory, const std::string& reference);
    BlockMaterial ReadMaterial(const std::filesystem::path& directory,
        const std::vector<std::string>& fields, const BlockId block);
    void UploadTextures(Renderer& renderer) const;
    void BindMaterials(Renderer& renderer) const;


    std::vector<TextureSource> textures;
    std::vector<BlockMaterial> materials;
    std::vector<TextureId> cracks;
};
