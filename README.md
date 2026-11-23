# Minecraft clone

Start in **`src/game/`**. This is the readable working area:

| File | Change here |
| --- | --- |
| `src/game/settings.h` | Movement multiplier, reach, mouse sensitivity, placement block, view radius |
| `src/game/game.cpp` | Controls and readable per-tick game decisions |
| `src/game/terrain.cpp` | Terrain layers and generation rules |
| `src/api/game.h` | Available world, player, terrain and debug operations |

Technical implementation lives in `src/simulation/` and `src/engine/`.
See [Gameplay guide](docs/GAMEPLAY_GUIDE.md), [Architecture](docs/ARCHITECTURE.md),
and [Agent instructions](AGENTS.md).

## Build and run

The game backend requires Windows, a DirectX 12-capable setup, CMake 3.24+, a C++23
compiler, and Python 3 for build-time asset staging and architecture checks. Python
is not needed to run the built game. Initialize the DirectXMath git submodule if absent.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/Engine.exe
```

Select your installed compiler/Ninja through a toolchain or your IDE. CLion can use
its existing Debug/Release profiles after reloading CMake. Build directories are ignored.
For a multi-configuration generator, supply `--config Debug` to the build and
`-C Debug` to CTest, and run the executable from the configuration subdirectory.

A separate build can omit the application backend:

```powershell
cmake -S . -B build-headless -G Ninja -DENGINE_BUILD_GAME=OFF -DBUILD_TESTING=ON
cmake --build build-headless
ctest --test-dir build-headless --output-on-failure
```

The simulation checks create a world/catalog/player without a window or graphics/audio
device. On Windows the terrain-cache checks additionally use vendored DirectXMath,
without initializing DirectX. Other operating systems have not been validated.

MinGW builds copy their required `libwinpthread-1.dll` beside each executable, so launch
does not depend on the IDE's PATH. Keep the staged `assets/` directory beside the game.

## Existing controls and scope

WASD moves, double-tap W or press Ctrl while moving forward to sprint, Space jumps, left mouse mines, right mouse places,
and F4 toggles Survival/Creative. Placement defaults to stone. Current terrain is flat.

The runtime stages only textures and converted sounds referenced by its manifests;
the full original asset library remains in `assets/`. See [asset notes](assets/README.md)
for cube-preview limitations. The game saves world edits, player position, look direction and game mode on normal
exit, and resumes automatically on startup. Saves live in `saves/world.save` beside
the executable. Inventory, drops, health and complete Creative flight are not implemented.
