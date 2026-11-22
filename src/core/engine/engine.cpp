#include "engine.h"

#include "ticker.h"

Engine::Engine(
	Window& window, Renderer& renderer, PlayerController& playerController, Camera& camera)
	: window(window), renderer(renderer), camera(camera), playerController(playerController), blocks(renderer)
{
	blocks.LoadNamespace(
		std::filesystem::path(ENGINE_ASSET_DIRECTORY) / "default",
		"default"
	);
}

void Engine::Run() const
{
	Ticker ticker(TickDuration);

	while (window.ProcessMessages())
	{
		ticker.Update();
		playerController.UpdateLook();

		while (ticker.ConsumeTick())
			Tick();

		playerController.UpdateCamera(ticker.GetAlpha());
		Render();
	}
}

void Engine::Tick() const
{
	playerController.Update(TickDuration);
}

void Engine::Render() const
{
	const Cube cube(blocks.GetBlockId("default:block/stone"));

	renderer.BeginFrame(camera);

	renderer.Clear(
		0.0f / 255.0f,
		0.0f / 255.0f,
		0.0f / 255.0f
	);
	renderer.DrawCube(cube);

	renderer.EndFrame();
}
