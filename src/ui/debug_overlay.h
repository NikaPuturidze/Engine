#pragma once

#include <cstddef>
#include "ui_element.h"
#include "../graphics/renderer.h"
#include "../core/player/player_controller.h"
#include "../world/world.h"

class DebugOverlay : public UIElement
{
	public:
		DebugOverlay(Renderer& renderer, const PlayerController& playerController, const World& world);
		void SetRenderedBlocks(std::size_t count) { renderedBlocks = count; }
		void Render() const override;

	private:
		Renderer& renderer;
		const PlayerController& playerController;
		const World& world;
		std::size_t renderedBlocks = 0;
};
