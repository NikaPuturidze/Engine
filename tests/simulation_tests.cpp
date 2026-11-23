#include "api/game.h"
#include "simulation/game_session.h"
#include "simulation/world/block_raycast.h"
#include "engine/runtime/ticker.h"
#include "engine/platform/sprint_input.h"
#include <iostream>
#include <stdexcept>
#include <sstream>

void Check(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
class FloorGenerator final : public WorldGenerator
{
public:
    void Generate(Chunk& chunk) const override
    {
        for (int z=0;z<16;++z) for (int x=0;x<16;++x) chunk.SetBlock(x,0,z,1);
    }
};
void WorldQueries()
{
    World world(std::make_unique<FloorGenerator>(),0);
    Check(!world.TryGetBlock(0,0,0),"Unloaded must differ from air");
    world.Update(-0.1f,-0.1f,1);
    Check(world.GetBlock(-1,0,-1)==1,"Negative coordinates select correct chunk");
    Check(world.TryGetBlock(-1,1,-1)==Air,"Loaded empty cell is air");
    Check(world.SetBlock(-1,1,-1,2),"Negative-coordinate edit succeeds");
    world.Update(32,32,1);
    Check(!world.TryGetBlock(-1,1,-1),"Unloaded edited chunk is unavailable");
    world.Update(-0.1f,-0.1f,1);
    Check(world.GetBlock(-1,1,-1)==2,"Edits survive unload/reload within session");
    Check(!world.SetBlock(-1,256,-1,2),"Out-of-height edit rejected");
    auto hit=RaycastBlock(world,{.x = -0.5f,.y = 3,.z = -0.5f},{.x = 0,.y = -1,.z = 0},5);
    Check(hit && hit->y==1 && hit->normalY==1,"Raycast returns edited block and surface normal");
    Check(!RaycastBlock(world,{.x = 0.5f,.y = 3,.z = 0.5f},{.x = 0,.y = -1,.z = 0},5),"Raycast stops at unavailable terrain");
}
void Clock()
{
    Ticker ticker(0.05);
    ticker.Advance(0.12);
    int ticks=0; while (ticker.ConsumeTick()) ++ticks;
    Check(ticks==2 && std::abs(ticker.GetAlpha()-0.4f)<0.001f,"Clock catches up and interpolates");
    ticker.Advance(10);
    ticks=0; while (ticker.ConsumeTick()) ++ticks;
    Check(ticks==5,"Catch-up work is bounded to five ticks");
}
void Mining()
{
    auto grass=std::make_shared<BlockDefinition>(); grass->id=1; grass->maxDurability=0.6f;
    MiningProgress mining(grass,{.x = 0,.y = 0,.z = 0});
    for (int i=0;i<17;++i) Check(!mining.UpdateMining(GameMode::Survival,0.05f),"Grass cannot break early");
    Check(mining.UpdateMining(GameMode::Survival,0.05f),"Grass breaks on tick eighteen");
    grass=std::make_shared<BlockDefinition>(); grass->id=2; grass->isDestructible=false;
    MiningProgress bedrock(grass,{.x = 0,.y = 0,.z = 0});
    Check(!bedrock.UpdateMining(GameMode::Survival,10),"Survival cannot break indestructible blocks");
    Check(bedrock.UpdateMining(GameMode::Creative,0.05f),"Creative can break indestructible blocks");
}
BlockRegistry Catalog()
{
    BlockRegistry blocks;
    BlockDefinition stone; stone.maxDurability=1.5f; stone.requiresToolForDrops=true;
    blocks.Register("minecraft:block/stone",stone);
    BlockDefinition grass; grass.maxDurability=0.6f;
    blocks.Register("minecraft:block/grass_block",grass);
    return blocks;
}
GameDefinition Definition()
{
    GameDefinition d; d.settings.chunkRadius=1; d.settings.spawn={.x = 0.5f,.y = 100,.z = 0.5f};
    d.generateTerrain=[](Terrain& t) { t.FillLayer(0,"minecraft:block/stone"); t.FillLayer(1,"minecraft:block/stone"); t.FillLayer(2,"minecraft:block/grass_block"); };
    d.update=[](const Game& g,const InputActions& i) {
        g.player.SetMoveInput(i.move,i.sprinting);
        if (i.jumpPressed || i.jumpHeld) g.player.RequestJump();
        if (i.toggleModePressed) g.player.ToggleMode();
        g.player.SetMining(i.mineHeld,i.minePressed);
        if (i.placePressed) g.player.TryPlaceSelectedBlock();
    };
    return d;
}
void HeadlessGame()
{
    auto blocks=Catalog(); auto d=Definition();
    GameSession game(d,blocks);
    Check(game.GetPlayer().GetPosition().y==3,"Headless session spawns on terrain");
    game.SetLook(0,45);
    InputActions input; input.placePressed=true;
    game.Tick(input);
    Check(game.GetWorld().GetBlock(0,3,2)==blocks.GetBlockId("minecraft:block/stone"),"Readable placement command edits targeted adjacent block");
    Check(!game.TakeSounds().empty(),"Placement produces feedback without an audio device");
    input={}; input.jumpPressed=true;
    game.Tick(input);
    Check(std::abs(game.GetPlayer().GetPosition().y-3.42f)<0.001f,"Jump preserves first-tick displacement");

}
void LoadingBoundary()
{
    auto blocks=Catalog(); auto d=Definition(); d.settings.chunkRadius=0; d.settings.spawn={.x = 15.5f,.y = 100,.z = 0.5f};
    GameSession game(d,blocks); InputActions input; input.move={.x = 1,.y = 0};
    for (int i=0;i<100;++i) game.Tick(input);
    Check(game.GetPlayer().GetPosition().x<=15.7001f,"Unloaded terrain blocks movement instead of acting as air");
}
void SprintControls()
{
    SprintInput input;
    constexpr auto now = SprintInput::Clock::time_point{};
    input.SetForward(true, now);
    Check(!input.IsSprinting(), "A single W press walks");
    input.SetForward(true, now + std::chrono::milliseconds(20));
    Check(!input.IsSprinting(), "W auto-repeat must not start sprinting");
    input.SetForward(false, now + std::chrono::milliseconds(40));
    input.SetForward(true, now + std::chrono::milliseconds(100));
    Check(input.IsSprinting(), "Double-tap W starts sprinting");
    input.SetForward(false);
    Check(!input.IsSprinting(), "Releasing W ends sprinting");
    input.Reset();
    input.SetForward(true, now);
    input.SetSprintKey(true);
    Check(input.IsSprinting(), "Ctrl while holding W starts sprinting");
    input.SetSprintKey(false);
    Check(input.IsSprinting(), "Releasing Ctrl retains active forward sprint");
    input.SetForward(false);
    input.SetForward(true, now + std::chrono::seconds(1));
    Check(!input.IsSprinting(), "Released Ctrl does not start the next forward sprint");
    input.Reset();
    input.SetSprintKey(true);
    Check(!input.IsSprinting(), "Ctrl alone does not sprint");
    input.SetForward(true, now);
    Check(input.IsSprinting(), "Holding Ctrl before W also starts sprinting");
    input.Cancel();
    Check(!input.IsSprinting(), "Backward cancellation ends sprinting");
    input.Reset();
    input.SetForward(true, now);
    Check(!input.IsSprinting(), "Focus reset clears held Ctrl and double-tap state");
}
float MeasureGroundSpeed(bool sprinting)
{
    auto blocks = Catalog();
    GameSession game(Definition(), blocks);
    InputActions input;
    input.move.y = 1;
    input.sprinting = sprinting;
    // Allow acceleration to settle, then measure world-space travel over two seconds.
    for (int i = 0; i < 80; ++i) { game.StreamWorld(); game.Tick(input); }
    const auto start = game.GetPlayer().GetPosition();
    for (int i = 0; i < 40; ++i) { game.StreamWorld(); game.Tick(input); }
    return Distance(start, game.GetPlayer().GetPosition()) / 2.0f;
}
void GroundSpeeds()
{
    const float walking = MeasureGroundSpeed(false);
    const float sprinting = MeasureGroundSpeed(true);
    std::cout << "Measured walking: " << walking << " blocks/s; sprinting: " << sprinting << " blocks/s\n";
    Check(std::abs(walking - 4.317f) < 0.002f, "Normal-ground walking matches Minecraft reference speed");
    Check(std::abs(sprinting - 5.612f) < 0.002f, "Normal-ground sprinting matches Minecraft reference speed");
    Check(std::abs(sprinting / walking - 1.3f) < 0.001f, "Sprinting is thirty percent faster");
}
static void SavedSessions()
{
    auto blocks = Catalog();
    auto definition = Definition();
    definition.settings.spawn = {-0.5f, 100, -0.5f};
    definition.settings.chunkRadius = 0;
    definition.update = [](const Game& game, const InputActions&)
    {
        game.world.SetBlock({-1, 0, -1}, Air);
        game.world.SetBlock({-2, 2, -2}, game.world.FindBlock("minecraft:block/stone"));
    };
    GameSession original(definition, blocks);
    original.SetLook(123, -27);
    original.Tick({});
    auto snapshot = original.CaptureSave();
    snapshot.mode = GameMode::Creative;
    // Capture includes edits even after their chunk leaves the streaming radius.
    snapshot.position = {64.25f, 12.5f, -32.75f};
    GameSession distant(definition, blocks, &snapshot);
    distant.StreamWorld();
    const auto unloaded = distant.CaptureSave();
    std::stringstream file;
    WriteSavedGame(file, unloaded, blocks);

    BlockRegistry reordered;
    reordered.Register("test:extra", {});
    reordered.Register("minecraft:block/stone", {});
    reordered.Register("minecraft:block/grass_block", {});
    const auto loaded = ReadSavedGame(file, reordered);
    GameSession restored(definition, reordered, &loaded);
    Check(Distance(restored.GetPlayer().GetPosition(), snapshot.position) == 0,
          "Resume restores exact position instead of spawning on the ground");
    Check(restored.Yaw() == 123 && restored.LookDirection().y > 0,
          "Resume restores look direction");
    Check(restored.GetPlayer().GetGameMode() == GameMode::Creative, "Resume restores mode");
    auto near = restored.CaptureSave();
    near.position = {-0.5f, 4, -0.5f};
    GameSession edited(definition, reordered, &near);
    Check(edited.GetWorld().TryGetBlock(-1, 0, -1) == Air, "Mined air survives save and reload");
    Check(edited.GetWorld().TryGetBlock(-2, 2, -2) == reordered.GetBlockId("minecraft:block/stone"),
          "Placed blocks survive unloaded chunks and changed registry IDs");

    for (const auto text : {"ENGINE_SAVE 99", "ENGINE_SAVE 1\n0 4 0 0 0 0\n1\n\"missing:block\"\n0\n",
                            "ENGINE_SAVE 1\n0 4 0 0 0 0\n0\n1\n0 0 0 256 0 0\n"})
    {
        bool rejected = false;
        try
        {
            std::istringstream invalid(text);
            ReadSavedGame(invalid, blocks);
        }
        catch (const std::exception&) { rejected = true; }
        Check(rejected, "Unsupported, missing-resource and corrupt saves are rejected");
    }
}

int main()
{
    try { WorldQueries(); Clock(); Mining(); HeadlessGame(); LoadingBoundary(); SprintControls(); GroundSpeeds(); SavedSessions(); std::cout<<"Simulation checks passed\n"; }
    catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
