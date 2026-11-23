#include "api/game.h"

void GenerateTerrain(const Terrain& terrain)
{
    terrain.FillLayer(0, "minecraft:block/stone");
    terrain.FillLayer(1, "minecraft:block/dirt");
    terrain.FillLayer(2, "minecraft:block/iron_ore");
    terrain.FillLayer(3, "minecraft:block/copper_ore");
    terrain.FillLayer(4, "minecraft:block/end_rod");
}
