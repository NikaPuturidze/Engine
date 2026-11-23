#include "api/game.h"

void GenerateTerrain(Terrain& terrain)
{
    terrain.FillLayer(0, "minecraft:block/stone");
    terrain.FillLayer(1, "minecraft:block/stone");
    terrain.FillLayer(2, "minecraft:block/grass_block");
}
