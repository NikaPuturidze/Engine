#include "api/game.h"
#include "engine/platform/input.h"
#include "engine/graphics/renderer.h"
#include "engine/assets/block_assets.h"
#include "engine/audio/sound_player.h"
#include "engine/runtime/ticker.h"
#include "engine/ui/debug_overlay.h"
#include "engine/ui/target_overlay.h"
#include "engine/ui/crosshair.h"
#include "simulation/game_session.h"
#include <filesystem>
#include <system_error>

namespace
{
std::filesystem::path AssetDirectory()
{
    std::vector<wchar_t> path(260);
    for (;;)
    {
        DWORD length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        if (!length) throw std::system_error(GetLastError(),std::system_category(),"Executable directory");
        if (length<path.size()) return std::filesystem::path(std::wstring(path.data(),length)).parent_path()/"assets/minecraft";
        path.resize(path.size()*2);
    }
}
class Application
{
public:
    explicit Application(GameDefinition definition)
        : settings(definition.settings), renderer(window), input(window)
    {
        const auto directory=AssetDirectory();
        assets.Load(directory,blocks);
        assets.Upload(renderer);
        sounds.LoadEvents(directory);
        session=std::make_unique<GameSession>(definition,blocks);
        debug=std::make_unique<DebugOverlay>(renderer,*session,session->GetWorld());
        target=std::make_unique<TargetOverlay>(renderer,camera,session->GetWorld(),blocks);
        crosshair=std::make_unique<Crosshair>(renderer,directory/"textures/gui/sprites/hud/crosshair.png");
    }
    void Run()
    {
        Ticker ticker(SimulationTickSeconds);
        while (window.ProcessMessages())
        {
            session->StreamWorld();
            const auto look=input.ConsumeLook();
            camera.Rotate(look.x*settings.mouseSensitivity,look.y*settings.mouseSensitivity);
            session->SetLook(camera.GetYaw(),camera.GetPitch());
            ticker.Update();
            while (ticker.ConsumeTick()) session->Tick(input.ConsumeTick());
            const auto p=session->CameraPosition(ticker.GetAlpha());
            camera.SetPosition(p.x,p.y,p.z);
            for (const auto& cue:session->TakeSounds())
                if (cue.event.empty()) sounds.Play(cue.variants,cue.volume,cue.pitch);
                else sounds.PlayEvent(cue.event,cue.volume,cue.pitch);
            sounds.Update();
            Render();
        }
    }
private:
    GameSettings settings;
    Window window;
    Renderer renderer;
    Input input;
    Camera camera;
    BlockRegistry blocks;
    BlockAssets assets;
    SoundPlayer sounds;
    std::unique_ptr<GameSession> session;
    std::unique_ptr<DebugOverlay> debug;
    std::unique_ptr<TargetOverlay> target;
    std::unique_ptr<Crosshair> crosshair;
    void Render()
    {
        const auto& world=session->GetWorld();
        renderer.BeginFrame(camera,world.GetChunkRadius()*Chunk::Width-2.0f);
        renderer.Clear(0.3f,0.6f,0.9f);
        const auto& mining=session->Mining();
        renderer.SetBlockCracking(mining ? mining->GetPosition() : BlockPosition{},
            mining ? assets.CrackTexture(mining->GetProgress()) : std::nullopt);
        debug->SetRenderedBlocks(renderer.DrawWorld(world));
        renderer.DrawDebug(session->Debug().Lines());
        if (settings.showDebug) debug->Render();
        target->Render(); crosshair->Render();
        renderer.EndFrame();
    }
};
}
int RunGame(GameDefinition definition)
{
    try { Application application(std::move(definition)); application.Run(); return 0; }
    catch (const std::exception& error)
    {
        MessageBoxA(nullptr,error.what(),"Game failed",MB_OK|MB_ICONERROR);
        return 1;
    }
}
