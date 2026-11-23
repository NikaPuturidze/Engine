#pragma once

#include <cstddef>
#include "../graphics/renderer.h"
#include "../core/player/player_controller.h"
#include "../world/world.h"

class DebugOverlay
{
	public:
		DebugOverlay(Renderer& renderer, const PlayerController& playerController, const World& world);
		void Render(std::size_t renderedBlocks) const;

	private:
		Renderer& renderer;
		const PlayerController& playerController;
		const World& world;
};
