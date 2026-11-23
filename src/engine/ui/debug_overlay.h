#pragma once

#include <cstddef>
#include "engine/ui/ui_element.h"
#include "engine/graphics/renderer.h"
#include "simulation/game_session.h"
#include "simulation/world/world.h"

class DebugOverlay : public UIElement
{
	public:
		DebugOverlay(Renderer& renderer, const GameSession& session, const World& world);
		void SetRenderedBlocks(const std::size_t count) { renderedBlocks = count; }
		void Render() const override;

	private:
		Renderer& renderer;
		const GameSession& session;
		const World& world;
		std::size_t renderedBlocks = 0;
};
