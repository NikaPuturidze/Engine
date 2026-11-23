#include "engine.h"
#include "ticker.h"
#include <system_error>
#include <vector>
#include <chrono>
#include <algorithm>

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
		sounds.LoadEvents(GetAssetDirectory() / "minecraft");
		blocks.LoadNamespace(
			GetAssetDirectory() / "minecraft",
			"minecraft"
		);

		auto generator = std::make_unique<FlatWorldGenerator>(
			blocks.GetBlockId("minecraft:block/grass_block"),
			blocks.GetBlockId("minecraft:block/stone")
		);

		world = std::make_unique<World>(std::move(generator));
		const auto position = playerController.GetPosition();
		world->Update(position.x, position.z, 9);
		playerController.Spawn(*world);
		debugOverlay = std::make_unique<DebugOverlay>(renderer, playerController, *world);
		targetOverlay = std::make_unique<TargetOverlay>(renderer, camera, *world, blocks);
		crosshair = std::make_unique<Crosshair>(renderer,
			GetAssetDirectory() / "minecraft/textures/gui/sprites/hud/crosshair.png");
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
	auto lastFrame = std::chrono::steady_clock::now();

	while (window.ProcessMessages())
	{
		const auto now = std::chrono::steady_clock::now();
		const float deltaSeconds = (std::min)(std::chrono::duration<float>(now - lastFrame).count(), 0.1f);
		lastFrame = now;
		const auto position = playerController.GetPosition();
		world->Update(position.x, position.z);
		ticker.Update();
		playerController.UpdateLook();

		while (ticker.ConsumeTick())
			Tick();

		playerController.UpdateCamera(ticker.GetAlpha());
		playerController.UpdateInteraction(*world, blocks, sounds, blocks.GetBlockId("minecraft:block/stone"), deltaSeconds);
		sounds.Update();
		Render();
	}
}

void Engine::Tick() const
{
	playerController.Update(TickDuration, *world);
	playerController.UpdateLandingSounds(*world, blocks, sounds);
	playerController.UpdateFootsteps(TickDuration, *world, blocks, sounds);
}

void Engine::Render() const
{
	renderer.BeginFrame(camera, world->GetChunkRadius() * Chunk::Width - 2.0f);
	renderer.Clear(0.3f, 0.6f, 0.9f);
	const auto& breaking = playerController.GetBreakingBlock();
	renderer.SetBlockCracking(breaking ? breaking->GetPosition() : BlockPosition{},
		breaking ? breaking->GetCrackingTexture() : std::nullopt);
	const std::size_t renderedBlocks = renderer.DrawWorld(*world);

	debugOverlay->SetRenderedBlocks(renderedBlocks);
	debugOverlay->Render();
	crosshair->Render();
	targetOverlay->Render();
	renderer.EndFrame();
}
