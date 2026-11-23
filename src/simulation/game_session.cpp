#include "simulation/game_session.h"
#include "simulation/world/block_raycast.h"
#include <stdexcept>

static_assert(Terrain::Width == Chunk::Width && Terrain::Depth == Chunk::Depth && Terrain::Height == Chunk::Height);

namespace
{
class CallbackGenerator final : public WorldGenerator
{
public:
    CallbackGenerator(std::function<void(Terrain&)> callback, const BlockRegistry& blocks)
        : callback(std::move(callback)), blocks(blocks) {}
    void Generate(Chunk& chunk) const override
    {
        Terrain terrain(chunk, blocks);
        callback(terrain);
    }
private:
    std::function<void(Terrain&)> callback;
    const BlockRegistry& blocks;
};
}

GameSession::GameSession(const GameDefinition& definition, const BlockRegistry& blocks)
    : definition(definition), blocks(blocks),
      world(std::make_unique<CallbackGenerator>(definition.generateTerrain, blocks), definition.settings.chunkRadius),
      player(definition.settings.spawn), worldAccess(world, blocks), playerAccess(*this),
      previousPosition(player.GetPosition()), placementBlock(blocks.GetBlockId(definition.settings.placementBlock))
{
    if (!definition.generateTerrain || !definition.update) throw std::invalid_argument("Game callbacks are required");
    const auto& s=definition.settings;
    if (!std::isfinite(s.interactionReach) || s.interactionReach<=0 ||
        !std::isfinite(s.movementMultiplier) || s.movementMultiplier<0 ||
        !std::isfinite(s.mouseSensitivity) || s.mouseSensitivity<0 ||
        !std::isfinite(s.spawn.x) || !std::isfinite(s.spawn.y) || !std::isfinite(s.spawn.z))
        throw std::invalid_argument("Game settings must have finite, valid values");
    world.Update(s.spawn.x, s.spawn.z, 9);
    player.Spawn(world);
    previousPosition=player.GetPosition();
}
void GameSession::StreamWorld()
{
    auto p=player.GetPosition();
    world.Update(p.x,p.z);
}
void GameSession::SetLook(const float yawDegrees, const float pitchDegrees)
{
    yaw=yawDegrees; pitch=pitchDegrees;
}
void GameSession::Tick(const InputActions& input)
{
    constexpr float seconds = static_cast<float>(SimulationTickSeconds);
    movement={}; sprint=jump=mineHeld=minePressed=false;
    debug.Clear();
    Game game{.world = worldAccess,.player = playerAccess,.debug = debug};
    definition.update(game,input);
    previousPosition=player.GetPosition();
    player.Move(movement.x,movement.y,yaw,seconds,sprint,definition.settings.movementMultiplier);
    if (jump) player.Jump();
    player.Update(seconds,world);
    const auto displacement=player.GetPosition()-previousPosition;
    horizontalSpeed=std::hypot(displacement.x,displacement.z)/seconds;
    UpdateMovementSounds(seconds);
    UpdateMining(seconds);
}
Vec3 GameSession::CameraPosition(const float alpha) const
{
    return Lerp(previousPosition,player.GetPosition(),alpha)+Vec3{.x = 0,.y = Player::EyeHeight,.z = 0};
}
void GameSession::UpdateMining(const float seconds)
{
    creativeCooldown=std::max(0.0f,creativeCooldown-seconds);
    const bool creative=player.GetGameMode()==GameMode::Creative;
    if (!mineHeld && !minePressed) { mining.reset(); return; }
    if (creative && !minePressed && creativeCooldown>0) { mining.reset(); return; }
    const auto hit=RaycastBlock(world,player.GetPosition()+Vec3{.x = 0,.y = Player::EyeHeight,.z = 0},LookDirection(),definition.settings.interactionReach);
    if (!hit) { mining.reset(); return; }
    const BlockPosition position{.x = hit->x,.y = hit->y,.z = hit->z};
    if (!mining || mining->GetPosition()!=position || mining->GetId()!=hit->block)
    {
        mining.emplace(blocks.GetBlockDefinition(hit->block),position);
        hitSoundTime=0;
    }
    if (!creative && !mining->IsDestructible()) return;
    hitSoundTime-=seconds;
    if (!creative && hitSoundTime<=0)
    {
        sounds.push_back({.variants = mining->GetDefinition().hittingSounds,.volume = 0.25f,.pitch = 0.5f, .event = {}});
        hitSoundTime+=0.2f;
    }
    if (mining->UpdateMining(player.GetGameMode(),seconds,player.IsGrounded()))
    {
        if (world.SetBlock(hit->x,hit->y,hit->z,Air))
            sounds.push_back({.variants = mining->GetDefinition().breakingSounds,.volume = 1,.pitch = 0.8f, .event = {}});
        mining.reset();
        if (creative) creativeCooldown=0.2f;
    }
}
PlaceResult GameSession::PlaceSelected()
{
    const auto origin=player.GetPosition()+Vec3{.x = 0,.y = Player::EyeHeight,.z = 0};
    const auto hit=RaycastBlock(world,origin,LookDirection(),definition.settings.interactionReach);
    if (!hit) return PlaceResult::OutOfReach;
    if (!hit->normalX && !hit->normalY && !hit->normalZ) return PlaceResult::Occupied;
    const BlockPosition p{.x = hit->x+hit->normalX,.y = hit->y+hit->normalY,.z = hit->z+hit->normalZ};
    if (p.y<0 || p.y>=Chunk::Height) return PlaceResult::TerrainUnavailable;
    const auto block=world.TryGetBlock(p.x,p.y,p.z);
    if (!block) return PlaceResult::TerrainUnavailable;
    if (*block!=Air) return PlaceResult::Occupied;
    if (!blocks.Contains(placementBlock) || placementBlock==Air) return PlaceResult::InvalidBlock;
    if (!player.PlaceBlock(world,p.x,p.y,p.z,placementBlock)) return PlaceResult::IntersectsPlayer;
    sounds.push_back({.variants = blocks.GetBlockDefinition(placementBlock)->placingSounds,.volume = 1,.pitch = 0.8f, .event = {}});
    return PlaceResult::Placed;
}
void GameSession::UpdateMovementSounds(const float seconds)
{
    const auto fall=player.ConsumeLandingSound();
    const auto landing=player.ConsumeLandingBlock();
    if (fall!=FallSound::None)
    {
        sounds.push_back({.variants = {},.volume = 1,.pitch = 1,.event = fall==FallSound::Big ? "entity.player.big_fall" : "entity.player.small_fall"});
        if (landing!=Air) sounds.push_back({.variants = blocks.GetBlockDefinition(landing)->fallingSounds,.volume = 0.5f,.pitch = 0.75f, .event = {}});
    }
    if (!footsteps.Advance(horizontalSpeed*seconds,player.IsGrounded())) return;
    const auto p=player.GetPosition();
    const auto id=world.GetBlock(static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y-0.05f)),static_cast<int>(std::floor(p.z)));
    if (id!=Air) sounds.push_back({.variants = blocks.GetBlockDefinition(id)->movementSounds,.volume = 0.15f,.pitch = 1, .event = {}});
}

Vec3 PlayerAccess::Position() const { return session.player.GetPosition(); }
Bounds PlayerAccess::GetBounds() const
{
    return session.player.GetBounds();
}
GameMode PlayerAccess::Mode() const { return session.player.GetGameMode(); }
void PlayerAccess::SetMoveInput(const Vec2 direction, const bool sprinting) const
{
    session.movement=direction; session.sprint=sprinting;
}
void PlayerAccess::RequestJump() const { session.jump=true; }
void PlayerAccess::ToggleMode() const
{
    session.player.SetGameMode(Mode()==GameMode::Survival ? GameMode::Creative : GameMode::Survival);
    session.mining.reset(); session.hitSoundTime=session.creativeCooldown=0;
}
void PlayerAccess::SetMining(const bool held, const bool pressed) const
{ session.mineHeld=held; session.minePressed=pressed; }
PlaceResult PlayerAccess::TryPlaceSelectedBlock() const { return session.PlaceSelected(); }

std::optional<BlockId> WorldAccess::GetBlock(const BlockPosition p) const { return world.TryGetBlock(p.x,p.y,p.z); }
BlockId WorldAccess::FindBlock(const std::string_view name) const { return blocks.GetBlockId(name); }
bool WorldAccess::SetBlock(const BlockPosition p, const BlockId block) const
{
    return blocks.Contains(block) && world.SetBlock(p.x,p.y,p.z,block);
}
void Terrain::FillLayer(const int y, const std::string_view block) const
{
    if (y<0 || y>=Height) throw std::out_of_range("Terrain layer is outside chunk height");
    const auto id=blocks.GetBlockId(block);
    for (int z=0;z<Depth;++z) for (int x=0;x<Width;++x) chunk.SetBlock(x,y,z,id);
}
void Terrain::SetBlock(const BlockPosition p, const std::string_view block) const
{
    if (p.x<0 || p.x>=Width || p.y<0 || p.y>=Height || p.z<0 || p.z>=Depth)
        throw std::out_of_range("Terrain position is outside chunk");
    chunk.SetBlock(p.x,p.y,p.z,blocks.GetBlockId(block));
}
int Terrain::WorldX(const int x) const { return chunk.GetChunkX()*Width+x; }
int Terrain::WorldZ(const int z) const { return chunk.GetChunkZ()*Depth+z; }
void DebugDraw::DrawPoint(const Vec3 p, const Color color, const float size)
{
    lines.push_back({.from = p-Vec3{.x = size,.y = 0,.z = 0},.to = p+Vec3{.x = size,.y = 0,.z = 0},.color = color});
    lines.push_back({.from = p-Vec3{.x = 0,.y = size,.z = 0},.to = p+Vec3{.x = 0,.y = size,.z = 0},.color = color});
    lines.push_back({.from = p-Vec3{.x = 0,.y = 0,.z = size},.to = p+Vec3{.x = 0,.y = 0,.z = size},.color = color});
}
void DebugDraw::DrawBox(Bounds b, const Color color)
{
    const auto a=b.minimum, c=b.maximum;
    const Vec3 p[]={{.x = a.x,.y = a.y,.z = a.z},{.x = c.x,.y = a.y,.z = a.z},{.x = c.x,.y = c.y,.z = a.z},{.x = a.x,.y = c.y,.z = a.z},
                    {.x = a.x,.y = a.y,.z = c.z},{.x = c.x,.y = a.y,.z = c.z},{.x = c.x,.y = c.y,.z = c.z},{.x = a.x,.y = c.y,.z = c.z}};
    constexpr int edges[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for (auto& edge:edges) lines.push_back({.from = p[edge[0]],.to = p[edge[1]],.color = color});
}
