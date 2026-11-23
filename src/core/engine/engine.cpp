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
	const auto isSolid = [](int x, int y, int z)
	{
		return (x == 0 || x == 1) && y == 0 && z == 4;
	};

	renderer.BeginFrame(camera);

	renderer.Clear(
		0.0f / 255.0f,
		0.0f / 255.0f,
		0.0f / 255.0f
	);

	for (int x = 0; x < 2; ++x)
	{
		Cube cube(blocks.GetBlockId("default:block/grass"));
		cube.x = static_cast<float>(x);
		renderer.DrawCube(cube, GetVisibleBlockFaces(x, 0, 4, isSolid));
	}

	renderer.EndFrame();
}
