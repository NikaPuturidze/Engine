# Asset library

The full imported source library is kept here. Builds use `tools/stage_assets.py`
to copy only manifest-referenced resources to the executable's asset directory.
See [architecture](../docs/ARCHITECTURE.md) for ownership and staging rules.

`minecraft/` replaces the former `default/` namespace. It contains the full
asset folder supplied from the local Minecraft assets, with original files
preserved. The engine starts with `minecraft:block/grass_block` and
`minecraft:block/stone`.

Block catalogs and converted sound files are included with the assets.
Running the game does not require Python or ffmpeg.

Grass tops and side overlays share a forest biome tint (`#79C05A`) by default.
The tint is stored in the block catalog; plains uses `#91BD59`. The engine has no biome map yet, so this is one world-wide tint rather
than biome blending. Rendering applies Minecraft's directional face shade
(top 1.0, north/south 0.8, east/west 0.6, bottom 0.5). Ambient occlusion,
skylight and time-of-day lighting are not implemented yet.

The included engine asset data contains:

- `minecraft/engine/blocks.tsv`: stable alphabetical block registrations,
  six face textures, durability, destructibility, openability, top tint, and
  weighted movement/break/place/hit sound variants.
  The last two columns specify whether harvesting requires a tool and the
  surface's fall sound variants.
- `minecraft/engine/sounds/`: mono 16-bit PCM WAV versions of referenced block
  sounds for XAudio2 playback. Original OGG files remain in `sounds/`.
- `minecraft/engine/import_report.json`: import counts and model limitations.
- `minecraft/engine/player_sounds.tsv`: player small- and big-fall sound events,
  resolved from the original `sounds.json`.

Hold the left mouse button to mine within six blocks. Progress resets when
releasing the button or changing targets. Cracks use the ten original
`destroy_stage` textures. Right click places stone and plays its placement
sound. Walking plays the sound of the surface below the grounded player.
Footsteps follow actual ground displacement, including coasting after movement
keys are released. The stride counter uses Minecraft's distance multiplier of
0.6 and retains its phase while slowing down or standing still. No minimum speed
or held-key check cuts off the final steps; stationary players emit no footsteps.
Horizontal travel while airborne also advances this counter, with sounds held
until ground contact. Sprint-jump landings can therefore play an ordinary walking
sound on the surface; jumping straight up does not. These footsteps are separate
from the small/big injury sounds used for damaging falls.

Players start in Survival mode. Press F4 to switch between Survival and Creative;
the current mode appears in the debug overlay. Survival uses hardness-based
mining and cannot break indestructible blocks. Creative removes blocks instantly,
including bedrock, without crack progress. Holding left click in Creative repeats
every 0.2 seconds; each new click can break immediately. Switching modes clears
the previous mining progress. Other mode-specific systems such as flight,
health, hunger, inventory and drops are not implemented yet.

Survival landings play the original player fall sounds when the calculated fall
damage is positive: small for 1–4 damage points, big for more than 4. Normal
whole-block drops of 4–7 blocks use the small sound; 8 or more use the big sound.
Ordinary jumps and drops of up to 3 blocks do not play an injury sound. Creative
does not play fall injury sounds. Only a damaging Survival landing also plays
the surface's fall sound, at volume 0.5 and pitch 0.75 (using default sound-type
volume/pitch). Ordinary jumps, safe drops, Creative landings, standing still and
spawning do not trigger fall audio. Edge landings use the block supporting the feet.
This is sound feedback only; health damage is not implemented yet. Fall distance
counts descending movement and excludes the final landing tick, as in Java.
Reference: https://raw.githubusercontent.com/mahtomedi/minecraft/main/src/main/java/net/minecraft/world/entity/LivingEntity.java

Bare-hand mining runs at 20 ticks per second. Each tick removes `1 / 30`
hardness units for blocks harvestable by hand or `1 / 100` for blocks that
require a harvesting tool. Stone (hardness 1.5) takes 150 ticks / 7.5 seconds;
grass blocks (hardness 0.6) take 18 ticks / 0.9 seconds. Mining while airborne
is five times slower. These are the normal Java Edition mining rules:
https://minecraft.wiki/w/Breaking. Loot drops and underwater/effect modifiers
are not implemented yet.

Asset files do not include Minecraft's game logic. Hardness, harvest requirements, material sound
groups, and openability currently use engine defaults in the block catalog. Sound
aliases and weights are imported; per-variant pitch and volume are not applied.
For the default sound type (volume/pitch 1), footsteps play at volume 0.15;
mining hits play at volume 0.25 and pitch 0.5 every 0.2 seconds; final breaks
and placement play at volume 1 and pitch 0.8. Surface landings use 0.5/0.75.
These are the Java playback modifiers, independent of the source recordings.

All non-air block types are registered, but the renderer currently draws cube
previews. It uses the first model from a blockstate; multipart models, model
rotations, custom geometry/UVs, texture layers, transparency sorting and block
state transitions need further rendering work. Animated textures currently
show their first frame. Entities, items, fluids, GUIs and other original assets
are present for later implementation. Chest/furnace opening remains a flag only.
