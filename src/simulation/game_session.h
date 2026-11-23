#pragma once
#include "api/game.h"
#include "simulation/world/mining_progress.h"
#include "simulation/world/world.h"
#include "simulation/world/block_registry.h"
#include "simulation/player/player.h"
#include "simulation/player/footstep_tracker.h"

// Feedback contains values/resource keys, never playback devices or texture IDs.
struct SoundCue
{
    std::vector<std::string> variants;
    float volume=1, pitch=1;
    std::string event;
};
class GameSession
{
public:
    GameSession(const GameDefinition& definition, const BlockRegistry& blocks);
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;
    GameSession(GameSession&&) = delete;
    GameSession& operator=(GameSession&&) = delete;
    void StreamWorld();
    void SetLook(float yawDegrees, float pitchDegrees);
    void Tick(const InputActions& input);
    Vec3 CameraPosition(float alpha) const;
    Vec3 LookDirection() const { return Forward(yaw, pitch); }
    float Yaw() const { float n=std::fmod(yaw,360.0f); return n<0 ? n+360 : n; }
    float Speed() const { return horizontalSpeed; }
    const Player& GetPlayer() const { return player; }
    const World& GetWorld() const { return world; }
    const std::optional<MiningProgress>& Mining() const { return mining; }
    const DebugDraw& Debug() const { return debug; }
    std::vector<SoundCue> TakeSounds() { auto result=std::move(sounds); sounds.clear(); return result; }
private:
    friend class PlayerAccess;
    GameDefinition definition;
    const BlockRegistry& blocks;
    World world;
    Player player;
    WorldAccess worldAccess;
    PlayerAccess playerAccess;
    DebugDraw debug;
    Vec3 previousPosition;
    Vec2 movement;
    bool sprint=false, jump=false, mineHeld=false, minePressed=false;
    float yaw=0, pitch=0, horizontalSpeed=0;
    float hitSoundTime=0, creativeCooldown=0;
    BlockId placementBlock;
    FootstepTracker footsteps;
    std::optional<MiningProgress> mining;
    std::vector<SoundCue> sounds;
    PlaceResult PlaceSelected();
    void UpdateMining(float seconds);
    void UpdateMovementSounds(float seconds);
};
