#pragma once

#include "../../graphics/renderer.h"
#include "../player/player_controller.h"
#include "../../world/block_registry.h"
#include "../../world/world.h"
#include "../../world/generator/flat_world_generator.h"
#include "../../ui/debug_overlay.h"
#include "../../ui/crosshair.h"
#include "../../ui/target_overlay.h"
#include "../../audio/sound_player.h"

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
		mutable SoundPlayer sounds;
		std::unique_ptr<World> world;
		std::unique_ptr<DebugOverlay> debugOverlay;
		std::unique_ptr<Crosshair> crosshair;
		std::unique_ptr<TargetOverlay> targetOverlay;

		void Tick() const;
		void Render() const;

		static constexpr double TickDuration = 1.0 / 20.0;
};
