#pragma once
#include "api/game_mode.h"
#include "api/input.h"
#include "api/settings.h"
#include "api/types.h"
#include <functional>
#include <optional>
#include <string_view>
#include <vector>
class World;
class Chunk;
class BlockRegistry;
class GameSession;
// Borrowed handle, valid during generation only. Positions are local to this chunk.
class Terrain
{
public:
    Terrain(Chunk& chunk, const BlockRegistry& blocks) : chunk(chunk), blocks(blocks) {}
    void FillLayer(int y, std::string_view block) const;
    void SetBlock(BlockPosition localPosition, std::string_view block) const;
    int WorldX(int localX) const;
    int WorldZ(int localZ) const;
    static constexpr int Width=16, Depth=16, Height=256;
private:
    Chunk& chunk;
    const BlockRegistry& blocks;
};
class WorldAccess
{
public:
    // nullopt means unavailable terrain, distinct from loaded air (Air).
    std::optional<BlockId> GetBlock(BlockPosition position) const;
    BlockId FindBlock(std::string_view name) const;
    // Raw editing for tools: bypasses player placement rules.
    bool SetBlock(BlockPosition position, BlockId block) const;
private:
    friend class GameSession;
    WorldAccess(World& world, const BlockRegistry& blocks) : world(world), blocks(blocks) {}
    World& world;
    const BlockRegistry& blocks;
};
class PlayerAccess
{
public:
    Vec3 Position() const;
    Bounds GetBounds() const;
    GameMode Mode() const;
    // Intent for this tick; acceleration/collision are applied after the callback.
    void SetMoveInput(Vec2 direction, bool sprinting=false) const;
    void RequestJump() const;
    void ToggleMode() const;
    void SetMining(bool held, bool pressed=false) const;
    // Immediate validated edit against the current simulation position and look direction.
    PlaceResult TryPlaceSelectedBlock() const;
private:
    friend class GameSession;
    explicit PlayerAccess(GameSession& session) : session(session) {}
    GameSession& session;
};
struct DebugLine { Vec3 from, to; Color color; };
class DebugDraw
{
public:
    // Markers remain visible until the next game tick.
    void DrawPoint(Vec3 position, Color color={}, float size=0.1f);
    void DrawBox(Bounds bounds, Color color={});
    const std::vector<DebugLine>& Lines() const { return lines; }
    void Clear() { lines.clear(); }
private:
    std::vector<DebugLine> lines;
};
// Borrowed handles: use inside update, do not retain across sessions.
struct Game { WorldAccess& world; PlayerAccess& player; DebugDraw& debug; };
struct GameDefinition
{
    GameSettings settings;
    std::function<void(Terrain&)> generateTerrain;
    std::function<void(Game&, const InputActions&)> update;
};
int RunGame(GameDefinition definition);
