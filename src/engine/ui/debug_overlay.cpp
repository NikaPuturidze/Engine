#include "engine/ui/debug_overlay.h"

#include <iomanip>
#include <locale>
#include <sstream>

DebugOverlay::DebugOverlay(Renderer& renderer,
    const GameSession& session, const World& world)
    : renderer(renderer), session(session), world(world)
{
}

void DebugOverlay::Render() const
{
    const auto position = session.GetPlayer().GetPosition();
    std::ostringstream information;
    information.imbue(std::locale::classic());
    information << "PLAYER POSITION\n" << std::fixed << std::setprecision(2)
        << "X: " << position.x << '\n'
        << "Y: " << position.y << '\n'
        << "Z: " << position.z << '\n'
        << "YAW: " << session.Yaw() << " DEG\n"
        << "SPEED: " << session.Speed() << " BLOCKS/S\n"
        << "MODE: " << (session.GetPlayer().GetGameMode() == GameMode::Creative ? "CREATIVE" : "SURVIVAL") << " (F4)\n"
        << "CHUNKS: " << world.GetChunks().size() << '\n'
        << "BLOCKS RENDERED: " << renderedBlocks;
    renderer.DrawOverlay(information.str());
}
