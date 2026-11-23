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
		world->Update(position.x, position.z, 9);
		playerController.Spawn(*world);
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
		const auto position = playerController.GetPosition();
		world->Update(position.x, position.z);
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
	playerController.Update(TickDuration, *world);
}

void Engine::Render() const
{
	renderer.BeginFrame(camera, world->GetChunkRadius() * Chunk::Width - 2.0f);
	renderer.Clear(0.3f, 0.6f, 0.9f);
	const std::size_t renderedBlocks = renderer.DrawWorld(*world);

	debugOverlay->SetRenderedBlocks(renderedBlocks);
	debugOverlay->Render();
	renderer.EndFrame();
}
