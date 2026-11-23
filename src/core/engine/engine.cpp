#include "engine.h"
#include "ticker.h"
#include <system_error>
#include <vector>

namespace
{
	std::filesystem::path GetAssetDirectory()
	{
		std::vector<wchar_t> executablePath(260);
		for (;;)
		{
			const DWORD length = GetModuleFileNameW(
				nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
			if (length == 0)
				throw std::system_error(GetLastError(), std::system_category(),
					"Failed to find executable directory");
			if (length < executablePath.size())
				return std::filesystem::path(std::wstring(executablePath.data(), length))
					.parent_path() / ENGINE_ASSET_DIRECTORY;
			executablePath.resize(executablePath.size() * 2);
		}
	}
}
#include <ranges>

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
			GetAssetDirectory() / "default",
			"default"
		);

		auto generator = std::make_unique<FlatWorldGenerator>(
			blocks.GetBlockId("default:block/grass"),
			blocks.GetBlockId("default:block/stone")
		);

		world = std::make_unique<World>(std::move(generator));
		const auto position = playerController.GetPosition();
		world->Update(position.x, position.z);
		debugOverlay = std::make_unique<DebugOverlay>(renderer, playerController, *world);
	}
	catch (const std::exception& error)
	{
		MessageBoxA(nullptr, error.what(), "Initialization failed", MB_OK);
		throw;
	}
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
	std::size_t renderedBlocks = 0;

	for (const auto& chunk : world->GetChunks() | std::views::values)
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
					const auto visibleFaces = GetVisibleBlockFaces(worldX, y, worldZ, isSolid);
					if (visibleFaces == 0) continue;

					Cube cube(block);

					cube.x = static_cast<float>(worldX);
					cube.y = static_cast<float>(y);
					cube.z = static_cast<float>(worldZ);

					renderer.DrawCube(
						cube,
						visibleFaces
					);
					++renderedBlocks;
				}
			}
		}
	}

	debugOverlay->Render(renderedBlocks);
	renderer.EndFrame();
}
