#pragma once

#include "../../graphics/renderer.h"
#include "../player/player_controller.h"
#include "../../world/block_registry.h"
#include "../../world/block_registry.h"

class Engine
{
	public:
		Engine(Window& window, Renderer& renderer, PlayerController& playerController, Camera& camera);
		void Run() const;

	private:
		Window& window;
		Renderer& renderer;
		Camera& camera;
		PlayerController& playerController;
		BlockRegistry blocks;

		void Tick() const;
		void Render() const;

		static constexpr double TickDuration = 1.0 / 20.0;
};
