#pragma once
#include "api/math.h"
// Held values persist. Pressed values are consumed once by the next simulation tick.
struct InputActions
{
    Vec2 move; // x: right, y: forward, in [-1, 1].
    bool sprinting=false;
    bool jumpHeld=false, jumpPressed=false;
    bool mineHeld=false, minePressed=false;
    bool placePressed=false, toggleModePressed=false;
};
