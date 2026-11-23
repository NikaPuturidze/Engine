# Working agreements for this Minecraft clone

Read `docs/ARCHITECTURE.md` before changing module dependencies or ownership.
Read `docs/GAMEPLAY_GUIDE.md` for normal feature work. `README.md` has build commands.

## Product and developer experience

We are building a Minecraft clone with a small, readable gameplay surface. The owner
should normally work in `src/game/` using named operations, plain vectors and settings.
Keep collision math, chunk indexing, streaming, mining calculations, Windows messages,
GPU resources and audio devices out of that area. Do not move complicated code into
game callbacks just because that is the easiest place to access a service.

## Enforced dependency direction

- `src/game/` includes only `api/`, `game/`, and standard C++ headers.
- `src/api/` includes only `api/` and standard C++ headers.
- `src/simulation/` includes only `simulation/`, `api/`, and standard C++ headers.
  It must run without a window, DirectX device, audio device, or asset-file loading.
- `src/engine/` adapts platform/assets/rendering/audio to simulation and API types.
  It must not include `game/` or contain decisions about default terrain/controls.
- Use source-root include paths, e.g. `api/math.h`. Do not bypass rules with relative
  includes, macro includes, globals, or a service locator.
- `tools/check_boundaries.py` runs as part of the build and through CTest. Update its
  rules only for a documented architectural decision, never to silence a violation.

## Design rules

- Add the smallest useful named operation. Avoid a general-purpose engine framework,
  an interface per class, an ECS, or an event bus without a concrete need.
- `Application` owns runtime services. `GameSession` owns authoritative gameplay.
  Renderers own derived CPU meshes/GPU data. World storage never uploads textures.
- Public positions are `Vec3` in blocks; block cells are integer `BlockPosition`.
  Angles are degrees. No `HWND`, `ComPtr`, matrices or texture IDs in gameplay APIs.
- Gameplay runs at 20 ticks/s. Held input persists; pressed input is consumed once.
  Mining uses simulation time. Mouse look and render interpolation run per frame.
- `TryPlaceSelectedBlock()` validates and edits immediately during the callback.
  Movement/jump/mining set intentions applied afterward. Preserve this ordering unless
  deliberately changing behavior with appropriate checks and updated documentation.
- Distinguish unloaded terrain from air. Use `TryGetBlock`/`WorldAccess::GetBlock`
  for gameplay decisions. Collision treats unloaded cells as blocking; targeting stops.
- `WorldAccess::SetBlock` is a raw tools/editing operation, not player placement.
- Numeric block IDs are session-local. A future save format must store a stable-name
  palette and a schema version; do not persist raw IDs as permanent identities.
- Borrowed API handles/callback arguments must not outlive the session or callback.
  Registry outlives session; no background work retains mutable chunk references.
- Keep GPU fence/resource lifetimes intact. Never overwrite or free an in-flight buffer.
- Do not modify vendored `external/` or bulk source assets for normal code changes.
  Runtime staging is driven by manifests; extend staging when a feature needs new assets.

## C++ code style

- Expand aggregate arguments with multiple named fields over separate lines. Put the
  opening brace on its own line inside the call, one field per line, and the closing
  brace and closing parenthesis on separate lines. For example:

  ```cpp
  return RunGame(
      {
          .settings = CreateSettings(),
          .generateTerrain = GenerateTerrain,
          .update = UpdateGame
      }
  );
  ```

- Use prefix `const` for parameters wherever applicable: `const Game& game`,
  `const InputActions& input`, and `const float seconds` for an unmodified value
  parameter in a definition. Keep references mutable when the function must mutate
  the referenced object; do not add `const` where it prevents the intended operation.
- Always check whether a function can be `static`. Make translation-unit-only free
  functions `static`; make member functions `static` when they need no instance state.
  Keep external linkage for functions called from other translation units. Do not
  introduce static storage for local variables merely to follow this convention.
- Always mark member functions `const` when they do not modify the object's logical
  state. Check const-correctness whenever adding or changing a member function; do not
  use `mutable` or `const_cast` merely to make a modifying function appear const.
- Leave a blank line between functions, including between a forward declaration and
  the next function declaration or definition. Do not pack multiple functions together.

## Verification and handoff

Build the game and run `ctest --test-dir <build-dir> --output-on-failure` after structural
changes. A headless build with `ENGINE_BUILD_GAME=OFF` checks simulation without runtime
initialization. Focus tests on behavior: coordinate boundaries, movement, interactions,
timing, change propagation and persistence when added. Avoid tests that copy algorithms.

For visible/input changes, smoke-test the application when available. Report exactly
what was verified; compiling is not visual verification. Keep docs accurate about
implemented features and remaining limitations. Update the architecture/guide when
changing an API contract or moving responsibilities. Do not leave migration scripts,
duplicate implementations, or unused abstraction layers in the repository.
