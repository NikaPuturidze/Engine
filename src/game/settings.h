#pragma once

#include "api/settings.h"

inline GameSettings CreateSettings()
{
    GameSettings settings;
    settings.movementMultiplier=1.0f;
    settings.interactionReach=6.0f;
    settings.placementBlock="minecraft:block/stone";
    return settings;
}
