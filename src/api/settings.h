#pragma once
#include "api/math.h"
#include <string>
inline constexpr double SimulationTickSeconds = 1.0 / 20.0;
struct GameSettings
{
    Vec3 spawn{.x = 0,.y = 100,.z = 0};
    int chunkRadius=32;
    float movementMultiplier=1;
    float interactionReach=6;
    float mouseSensitivity=0.1f;
    std::string placementBlock="minecraft:block/stone";
    bool showDebug=true;
};
