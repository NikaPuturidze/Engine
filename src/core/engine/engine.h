#pragma once

#include "../../graphics/renderer.h"
#include "../player/player_controller.h"
#include "../../world/block_registry.h"
#include "../../world/world.h"
#include "../../world/generator/flat_world_generator.h"

class Engine
{
	public:
		Engine(Window& window, Renderer& renderer, PlayerController& playerController, Camera& camera);
		void Run();

	private:
		Window& window;
		Renderer& renderer;
		Camera& camera;
		PlayerController& playerController;
		BlockRegistry blocks;
		std::unique_ptr<World> world;

		void Tick();
		void Render() const;

		static constexpr double TickDuration = 1.0 / 20.0;
};
