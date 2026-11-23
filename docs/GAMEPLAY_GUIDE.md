# Working on the game

The aim is to express game decisions without learning DirectX, Windows messages,
chunk storage, or collision algorithms. Your normal working area is `src/game/`.

## Change settings

Edit `CreateSettings()` in `src/game/settings.h`:

```cpp
settings.movementMultiplier = 1.25f;
settings.interactionReach = 6.0f;
settings.placementBlock = "minecraft:block/grass_block";
settings.chunkRadius = 16;
```

Movement multiplier changes input acceleration, not velocity directly; friction and
collision still apply. Chunk radius controls a square of `(2 * radius + 1)^2` columns.
Do not increase it without considering CPU memory, generation time and GPU uploads.

## Change terrain

Edit `GenerateTerrain()` in `src/game/terrain.cpp`:

```cpp
terrain.FillLayer(0, "minecraft:block/stone");
terrain.FillLayer(1, "minecraft:block/grass_block");
terrain.SetBlock({4, 2, 4}, "minecraft:block/stone");
```

The game currently selects `GenerateTestTerrain()` in `src/game/game.cpp`. Both terrain
files compile together: `src/game/terrain_test.cpp` calls `GenerateTerrain()` for the
flat ground, then adds the catalog samples at y=4, starting at world (6, 4, 6).
Samples have five empty cells between them, with 36 columns along +X and rows along
+Z. The block-name list is a snapshot of `assets/minecraft/engine/blocks.tsv`.
Select `GenerateTerrain` in `game.cpp` to return to ground without the samples
(update its forward declaration too).

Each callback generates one 16×16 column, with heights from 0 through 255. Coordinates
are local to that column. `terrain.WorldX(x)` and `terrain.WorldZ(z)` provide world
coordinates for hills/noise rules. Invalid local coordinates throw. Empty chunks are
valid, but the spawn location must have supporting terrain. Generation is synchronous
and budgeted; avoid external I/O or mutable global state inside the callback.

## Controls and timing

Sprint by double-tapping W or pressing Ctrl while holding W (holding Ctrl before W
also works). Both routes use the same sprint acceleration. Releasing Ctrl keeps an
active sprint until forward movement ends; losing focus clears the input state.
On ordinary flat ground with default settings, walking settles at about 4.317 blocks/s
and sprinting at 5.612 blocks/s, matching the [Minecraft Wiki](https://minecraft.wiki/w/Sprinting).
These figures exclude sprint-jumping, effects and special surfaces.

`UpdateGame(Game&, const InputActions&)` runs once per 50 ms simulation tick.

```cpp
game.player.SetMoveInput(input.move, input.sprinting);
if (input.jumpPressed) game.player.RequestJump();
game.player.SetMining(input.mineHeld, input.minePressed);
if (input.placePressed) game.player.TryPlaceSelectedBlock();
```

Use `jumpHeld || jumpPressed` to retain the current held-jump behavior. Pressed fields
are consumed once, including when multiple simulation ticks run in a frame. Held fields
remain set until release/focus loss. Movement/jump/mining are tick intentions, reset
before every callback. Placement and mode switching take effect immediately during
the callback; placement checks the current position and non-interpolated look direction.
Do not repeat a pressed action from your own frame loop.

`TryPlaceSelectedBlock()` returns `Placed`, `Occupied`, `TerrainUnavailable`,
`OutOfReach`, `IntersectsPlayer`, or `InvalidBlock`. No hit currently reports OutOfReach,
including a ray that encounters unloaded terrain before finding a block. Successful
placement produces a sound cue; the game callback never loads or plays a sound file.

## Query or edit the world

```cpp
const auto cell = game.world.GetBlock({10, 3, 10});
if (cell && *cell == Air) {
    const auto stone = game.world.FindBlock("minecraft:block/stone");
    game.world.SetBlock({10, 3, 10}, stone);
}
```

No value means that the chunk is unavailable. `Air` means a loaded empty cell.
Outside the fixed vertical world bounds is known empty space; writes there fail.
Raw `SetBlock()` validates the ID and loaded target, but deliberately bypasses reach,
player overlap and feedback. Use it for tools or scripted edits, not player placement.
It returns false for invalid/unavailable targets or an unchanged block.

## Useful math and debug drawing

```cpp
Vec3 position = game.player.Position();
Vec3 direction = Normalize(Vec3{10, 5, 10} - position);
float distance = Distance(position, {10, 5, 10});
game.debug.DrawPoint(position + Vec3{0, 2, 3}, {1, 0, 0});
game.debug.DrawBox(game.player.GetBounds(), {0, 1, 0});
```

Positions and bounds use blocks. The player's position is the center of their feet.
`Normalize({})` returns zero; `Lerp(a,b,alpha)` does not clamp alpha. `Forward(yaw,pitch)`
uses degrees, +Z forward at yaw 0, and positive pitch downward. Rendering converts to
DirectX internally. Debug markers last until the next tick; redraw them to keep them.
They render as an overlay through terrain, with a bounded vertex capacity (excess
lines are omitted), and are intended for diagnostics rather than gameplay geometry.

## When a method is missing

Add a small operation to `src/api/`, implement its calculations in `src/simulation/`
or its presentation in `src/engine/`, then use it from `game/`. Document timing,
coordinates and failure behavior. Avoid obtaining the renderer, raw chunks or Windows
handles from the game callback. Keep related operations together instead of creating
one file or abstract interface for every method.
