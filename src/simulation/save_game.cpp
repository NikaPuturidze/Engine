#include "simulation/save_game.h"

#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <vector>

static void ValidatePlayer(const SavedGame& saved)
{
    const auto p = saved.position;
    // Leave ample room for integer chunk arithmetic and collision queries.
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
        std::abs(p.x) > 10000000 || std::abs(p.z) > 10000000 ||
        p.y < -10000 || p.y > 10000 || !std::isfinite(saved.yaw) ||
        !std::isfinite(saved.pitch) || std::abs(saved.yaw) > 360 ||
        std::abs(saved.pitch) > 89 ||
        (saved.mode != GameMode::Survival && saved.mode != GameMode::Creative))
        throw std::runtime_error("Invalid player state in save");
}

SavedGame ReadSavedGame(std::istream& input, const BlockRegistry& blocks)
{
    SavedGame saved;
    std::string magic;
    int version = 0, mode = 0;
    if (!(input >> magic >> version) || magic != "ENGINE_SAVE" || version != 1)
        throw std::runtime_error("Unsupported or corrupt save file");
    if (!(input >> saved.position.x >> saved.position.y >> saved.position.z >>
          saved.yaw >> saved.pitch >> mode) || mode < 0 || mode > 1)
        throw std::runtime_error("Invalid saved player");
    saved.mode = mode == 0 ? GameMode::Survival : GameMode::Creative;
    ValidatePlayer(saved);

    std::size_t paletteSize = 0, count = 0;
    if (!(input >> paletteSize) || paletteSize > 65536)
        throw std::runtime_error("Invalid save palette");
    std::vector<BlockId> palette;
    for (std::size_t i = 0; i < paletteSize; ++i)
    {
        std::string name;
        if (!(input >> std::quoted(name)) || name.size() > 256)
            throw std::runtime_error("Invalid saved block name");
        // Unknown names fail loading; never silently turn edited blocks into air.
        palette.push_back(blocks.GetBlockId(name));
    }
    if (!(input >> count) || count > 10000000)
        throw std::runtime_error("Invalid saved edit count");
    for (std::size_t i = 0; i < count; ++i)
    {
        int cx, cz, x, y, z;
        std::size_t block;
        if (!(input >> cx >> cz >> x >> y >> z >> block) ||
            cx < -625000 || cx > 625000 || cz < -625000 || cz > 625000 ||
            x < 0 || x >= Chunk::Width || z < 0 || z >= Chunk::Depth ||
            y < 0 || y >= Chunk::Height || block >= palette.size())
            throw std::runtime_error("Invalid saved block edit");
        if (!saved.edits[{cx, cz}].emplace(std::array{x, y, z}, palette[block]).second)
            throw std::runtime_error("Duplicate saved block edit");
    }
    input >> std::ws;
    if (!input.eof()) throw std::runtime_error("Unexpected data after save");
    return saved;
}

void WriteSavedGame(std::ostream& output, const SavedGame& saved, const BlockRegistry& blocks)
{
    ValidatePlayer(saved);
    std::map<BlockId, std::size_t> palette;
    std::size_t count = 0;
    for (const auto& [chunk, edits] : saved.edits)
        for (const auto& [position, block] : edits)
        {
            if (!blocks.Contains(block)) throw std::runtime_error("Cannot save unknown block");
            palette.try_emplace(block, palette.size());
            ++count;
        }
    output << "ENGINE_SAVE 1\n" << std::setprecision(std::numeric_limits<float>::max_digits10)
           << saved.position.x << ' ' << saved.position.y << ' ' << saved.position.z << ' '
           << saved.yaw << ' ' << saved.pitch << ' '
           << (saved.mode == GameMode::Creative ? 1 : 0) << '\n' << palette.size() << '\n';
    std::vector<std::string_view> names(palette.size());
    for (const auto& [block, index] : palette) names[index] = blocks.GetBlockName(block);
    for (const auto name : names) output << std::quoted(std::string(name)) << '\n';
    output << count << '\n';
    for (const auto& [chunk, edits] : saved.edits)
        for (const auto& [position, block] : edits)
            output << chunk.first << ' ' << chunk.second << ' ' << position[0] << ' '
                   << position[1] << ' ' << position[2] << ' ' << palette.at(block) << '\n';
    if (!output) throw std::runtime_error("Failed writing save file");
}
