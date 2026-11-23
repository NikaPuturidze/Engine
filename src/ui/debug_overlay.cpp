#include "debug_overlay.h"

#include <iomanip>
#include <locale>
#include <sstream>

DebugOverlay::DebugOverlay(Renderer& renderer,
    const PlayerController& playerController, const World& world)
    : renderer(renderer), playerController(playerController), world(world)
{
}

void DebugOverlay::Render(const std::size_t renderedBlocks) const
{
    const auto position = playerController.GetPosition();
    std::ostringstream information;
    information.imbue(std::locale::classic());
    information << "PLAYER POSITION\n" << std::fixed << std::setprecision(2)
        << "X: " << position.x << '\n'
        << "Y: " << position.y << '\n'
        << "Z: " << position.z << '\n'
        << "CHUNKS: " << world.GetChunks().size() << '\n'
        << "BLOCKS RENDERED: " << renderedBlocks;
    renderer.DrawOverlay(information.str());
}
