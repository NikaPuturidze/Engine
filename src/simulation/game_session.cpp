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

    ValidateDefinition();
    GenerateInitialTerrain();
    SpawnPlayer();
}

void GameSession::ValidateDefinition() const
{
    if (!definition.generateTerrain || !definition.update)
        throw std::invalid_argument("Game callbacks are required");
    const auto& settings = definition.settings;
    if (!std::isfinite(settings.interactionReach) || settings.interactionReach <= 0 ||
        !std::isfinite(settings.movementMultiplier) || settings.movementMultiplier < 0 ||
        !std::isfinite(settings.mouseSensitivity) || settings.mouseSensitivity < 0 ||
        !std::isfinite(settings.spawn.x) || !std::isfinite(settings.spawn.y) || !std::isfinite(settings.spawn.z))
        throw std::invalid_argument("Game settings must have finite, valid values");
}

void GameSession::GenerateInitialTerrain()
{
    const auto spawn = definition.settings.spawn;
    world.Update(spawn.x, spawn.z, 9);
}

void GameSession::SpawnPlayer()
{
    player.Spawn(world);
    previousPosition = player.GetPosition();
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
    BeginTick();
    ApplyGameRules(input);
    MovePlayer(seconds);
    UpdateMovementSounds(seconds);
    UpdateMining(seconds);
}

void GameSession::BeginTick()
{
    movement = {};
    sprint = jump = mineHeld = minePressed = false;
    debug.Clear();
}

void GameSession::ApplyGameRules(const InputActions& input)
{
    Game game{worldAccess, playerAccess, debug};
    definition.update(game, input);
}

void GameSession::MovePlayer(const float seconds)
{
    previousPosition = player.GetPosition();
    player.Move(movement.x, movement.y, yaw, seconds, sprint, definition.settings.movementMultiplier);
    if (jump) player.Jump();
    player.Update(seconds, world);
    const auto displacement = player.GetPosition() - previousPosition;
    horizontalSpeed = std::hypot(displacement.x, displacement.z) / seconds;
}
Vec3 GameSession::CameraPosition(const float alpha) const
{
    return Lerp(previousPosition,player.GetPosition(),alpha)+Vec3{.x = 0,.y = Player::EyeHeight,.z = 0};
}
void GameSession::UpdateMining(const float seconds)
{
    creativeCooldown = std::max(0.0f, creativeCooldown - seconds);
    if (!CanMineThisTick())
    {
        mining.reset();
        return;
    }
    const auto hit = FindTargetBlock();
    if (!hit)
    {
        mining.reset();
        return;
    }
    SelectMiningTarget(*hit);
    const bool creative = player.GetGameMode() == GameMode::Creative;
    if (!creative && !mining->IsDestructible()) return;
    QueueMiningHitSound(seconds, creative);
    if (mining->UpdateMining(player.GetGameMode(), seconds, player.IsGrounded()))
        BreakMinedBlock(*hit, creative);
}

bool GameSession::CanMineThisTick() const
{
    if (!mineHeld && !minePressed) return false;
    return player.GetGameMode() != GameMode::Creative || minePressed || creativeCooldown <= 0;
}

std::optional<BlockHit> GameSession::FindTargetBlock() const
{
    const auto eye = player.GetPosition() + Vec3{0, Player::EyeHeight, 0};
    return RaycastBlock(world, eye, LookDirection(), definition.settings.interactionReach);
}

void GameSession::SelectMiningTarget(const BlockHit& hit)
{
    const BlockPosition position{hit.x, hit.y, hit.z};
    if (mining && mining->GetPosition() == position && mining->GetId() == hit.block) return;
    mining.emplace(blocks.GetBlockDefinition(hit.block), position);
    hitSoundTime = 0;
}

void GameSession::QueueMiningHitSound(const float seconds, const bool creative)
{
    hitSoundTime -= seconds;
    if (!creative && hitSoundTime <= 0)
    {
        QueueSound(mining->GetDefinition().hittingSounds, 0.25f, 0.5f);
        hitSoundTime += 0.2f;
    }
}

void GameSession::BreakMinedBlock(const BlockHit& hit, const bool creative)
{
    if (world.SetBlock(hit.x, hit.y, hit.z, Air))
        QueueSound(mining->GetDefinition().breakingSounds, 1, 0.8f);
    mining.reset();
    if (creative) creativeCooldown = 0.2f;
}
PlaceResult GameSession::PlaceSelected()
{
    const auto hit = FindTargetBlock();
    if (!hit) return PlaceResult::OutOfReach;
    if (!hit->normalX && !hit->normalY && !hit->normalZ) return PlaceResult::Occupied;
    const BlockPosition position{hit->x + hit->normalX, hit->y + hit->normalY, hit->z + hit->normalZ};
    const auto result = CheckPlacement(position);
    if (result != PlaceResult::Placed) return result;
    if (!player.PlaceBlock(world, position.x, position.y, position.z, placementBlock))
        return PlaceResult::IntersectsPlayer;
    QueueSound(blocks.GetBlockDefinition(placementBlock)->placingSounds, 1, 0.8f);
    return PlaceResult::Placed;
}

PlaceResult GameSession::CheckPlacement(const BlockPosition position) const
{
    if (position.y < 0 || position.y >= Chunk::Height) return PlaceResult::TerrainUnavailable;
    const auto block = world.TryGetBlock(position.x, position.y, position.z);
    if (!block) return PlaceResult::TerrainUnavailable;
    if (*block != Air) return PlaceResult::Occupied;
    if (!blocks.Contains(placementBlock) || placementBlock == Air) return PlaceResult::InvalidBlock;
    return PlaceResult::Placed;
}
void GameSession::UpdateMovementSounds(const float seconds)
{
    QueueLandingSounds();
    QueueFootstepSound(seconds);
}

void GameSession::QueueLandingSounds()
{
    const auto fall = player.ConsumeLandingSound();
    const auto landing = player.ConsumeLandingBlock();
    if (fall == FallSound::None) return;
    sounds.push_back(
        {
            .variants = {},
            .volume = 1,
            .pitch = 1,
            .event = fall == FallSound::Big ? "entity.player.big_fall" : "entity.player.small_fall"
        }
    );
    if (landing != Air) QueueSound(blocks.GetBlockDefinition(landing)->fallingSounds, 0.5f, 0.75f);
}

void GameSession::QueueFootstepSound(const float seconds)
{
    if (!footsteps.Advance(horizontalSpeed * seconds, player.IsGrounded())) return;
    const auto position = player.GetPosition();
    const auto block = world.GetBlock(
        static_cast<int>(std::floor(position.x)),
        static_cast<int>(std::floor(position.y - 0.05f)),
        static_cast<int>(std::floor(position.z))
    );
    if (block != Air) QueueSound(blocks.GetBlockDefinition(block)->movementSounds, 0.15f, 1);
}

void GameSession::QueueSound(const std::vector<std::string>& variants, const float volume, const float pitch)
{
    sounds.push_back(
        {
            .variants = variants,
            .volume = volume,
            .pitch = pitch,
            .event = {}
        }
    );
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
void DebugDraw::DrawBox(const Bounds& b, const Color color)
{
    const auto a=b.minimum, c=b.maximum;
    const Vec3 p[]={{.x = a.x,.y = a.y,.z = a.z},{.x = c.x,.y = a.y,.z = a.z},{.x = c.x,.y = c.y,.z = a.z},{.x = a.x,.y = c.y,.z = a.z},
                    {.x = a.x,.y = a.y,.z = c.z},{.x = c.x,.y = a.y,.z = c.z},{.x = c.x,.y = c.y,.z = c.z},{.x = a.x,.y = c.y,.z = c.z}};
    constexpr int edges[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for (auto& edge:edges) lines.push_back({.from = p[edge[0]],.to = p[edge[1]],.color = color});
}
