#include "engine.h"
#include "ticker.h"

Engine::Engine(
	Window& window,
	Renderer& renderer,
	PlayerController& playerController,
	Camera& camera)
	: window(window),
	  renderer(renderer),
	  camera(camera),
	  playerController(playerController),
	  blocks(renderer)
{
	try
	{
		blocks.LoadNamespace(
			std::filesystem::path(ENGINE_ASSET_DIRECTORY) / "default",
			"default"
		);

		auto generator = std::make_unique<FlatWorldGenerator>(
			blocks.GetBlockId("default:block/grass"),
			blocks.GetBlockId("default:block/stone")
		);

		world = std::make_unique<World>(std::move(generator));
		const auto position = playerController.GetPosition();
		world->Update(position.x, position.z);
	}
	catch (const std::exception& error)
	{
		MessageBoxA(nullptr, error.what(), "Initialization failed", MB_OK);
		throw;
	}
}

void Engine::Run()
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

void Engine::Tick()
{
	playerController.Update(TickDuration);
	const auto position = playerController.GetPosition();
	world->Update(position.x, position.z);
}

void Engine::Render() const
{
	const auto isSolid = [this](const int x, const int y, const int z)
	{
		return world->GetBlock(x, y, z) != 0;
	};

	renderer.BeginFrame(camera);
	renderer.Clear(0.3f, 0.6f, 0.9f);

	for (const auto& [coordinates, chunk] : world->GetChunks())
	{
		for (int y = 0; y < Chunk::Height; ++y)
		{
			for (int z = 0; z < Chunk::Depth; ++z)
			{
				for (int x = 0; x < Chunk::Width; ++x)
				{
					const BlockId block = chunk.GetBlock(x, y, z);
					if (block == 0) continue;
					const int worldX = chunk.GetChunkX() * Chunk::Width + x;
					const int worldZ = chunk.GetChunkZ() * Chunk::Depth + z;

					Cube cube(block);

					cube.x = static_cast<float>(worldX);
					cube.y = static_cast<float>(y);
					cube.z = static_cast<float>(worldZ);

					renderer.DrawCube(
						cube,
						GetVisibleBlockFaces(worldX, y, worldZ, isSolid)
					);
				}
			}
		}
	}
	renderer.EndFrame();
}
