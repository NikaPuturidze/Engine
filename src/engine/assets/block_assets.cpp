#include "engine/assets/block_assets.h"
#include "engine/graphics/renderer.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
namespace
{
std::vector<std::string> Split(const std::string& text, const char separator)
{
    std::vector<std::string> result; std::istringstream stream(text); std::string value;
    while (std::getline(stream,value,separator)) result.push_back(value);
    return result;
}
}
void BlockAssets::Load(const std::filesystem::path& directory, BlockRegistry& blocks)
{
    std::ifstream manifest(directory/"engine/blocks.tsv"); std::string line;
    if (!std::getline(manifest,line) || line!="engine-blocks-v3")
        throw std::runtime_error("Missing or invalid block manifest: "+directory.string());
    std::unordered_map<std::string,TextureId> ids;
    auto texture=[&](const std::string& reference)
    {
        if (auto found=ids.find(reference);found!=ids.end()) return found->second;
        TextureId id=textures.size()+1;
        textures.push_back({.id = id,.path = directory/"textures"/(reference+".png")});
        ids.emplace(reference,id); return id;
    };
    for (int stage=0;stage<10;++stage) cracks.push_back(texture("block/destroy_stage_"+std::to_string(stage)));
    while (std::getline(manifest,line))
    {
        if (line.empty()) continue;
        const auto f=Split(line,'\t');
        if (f.size()!=19) throw std::runtime_error("Invalid block manifest row: "+line);
        BlockDefinition definition;
        definition.maxDurability=std::stof(f[7]); definition.isDestructible=f[8]=="1";
        definition.isOpenable=f[9]=="1"; definition.requiresToolForDrops=f[17]=="1";
        definition.movementSounds=Split(f[13],','); definition.breakingSounds=Split(f[14],',');
        definition.placingSounds=Split(f[15],','); definition.hittingSounds=Split(f[16],',');
        definition.fallingSounds=Split(f[18],',');
        BlockMaterial material;
        material.block=blocks.Register("minecraft:block/"+f[0],std::move(definition));
        for (std::size_t face=0;face<6;++face) material.faces[face]=texture(f[1+face]);
        material.red=std::stof(f[10]); material.green=std::stof(f[11]); material.blue=std::stof(f[12]);
        if (f[0]=="grass_block") material.sideOverlay=texture("block/grass_block_side_overlay");
        materials.push_back(material);
    }
}
void BlockAssets::Upload(Renderer& renderer) const
{
    for (const auto& source:textures) renderer.RegisterTexture(Texture::LoadPng(source.id,source.path,true));
    for (const auto& m:materials)
    {
        renderer.BindTextures(m.block,m.faces[0],m.faces[4],m.faces[5],m.red,m.green,m.blue);
        renderer.BindFaceTextures(m.block,m.faces);
        if (m.sideOverlay) renderer.BindSideOverlay(m.block,m.sideOverlay);
    }
}
std::optional<TextureId> BlockAssets::CrackTexture(const float progress) const
{
    if (progress<=0 || progress>=1 || cracks.empty()) return std::nullopt;
    return cracks[std::min(static_cast<std::size_t>(progress*cracks.size()),cracks.size()-1)];
}
