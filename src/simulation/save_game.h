#pragma once

#include "api/game_mode.h"
#include "api/math.h"
#include "simulation/world/world.h"
#include "simulation/world/block_registry.h"
#include <iosfwd>

struct SavedGame
{
    Vec3 position;
    float yaw = 0, pitch = 0;
    GameMode mode = GameMode::Survival;
    World::Edits edits;
};

// Versioned stream format; block IDs are converted through a stable-name palette.
SavedGame ReadSavedGame(std::istream& input, const BlockRegistry& blocks);

void WriteSavedGame(std::ostream& output, const SavedGame& saved, const BlockRegistry& blocks);
