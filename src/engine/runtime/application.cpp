#include "api/game.h"
#include "engine/platform/input.h"
#include "engine/graphics/renderer.h"
#include "engine/assets/block_assets.h"
#include "engine/audio/sound_player.h"
#include "engine/runtime/ticker.h"
#include "engine/ui/debug_overlay.h"
#include "engine/ui/target_overlay.h"
#include "engine/ui/crosshair.h"
#include "simulation/game_session.h"
#include <filesystem>
#include <system_error>

namespace
{
	std::filesystem::path AssetDirectory()
	{
		std::vector<wchar_t> path(260);
		for (;;)
		{
			const DWORD length = GetModuleFileNameW(nullptr, path.data(), path.size());
			if (!length) throw std::system_error(GetLastError(), std::system_category(), "Executable directory");
			if (length < path.size())
				return std::filesystem::path(std::wstring(path.data(), length)).parent_path() /
					"assets/minecraft";
			path.resize(path.size() * 2);
		}
	}

	class Application
	{
		public:
			explicit Application(const GameDefinition& definition)
				: definition(definition), settings(definition.settings), renderer(window), input(window)
			{
			}

			void Run()
			{
				LoadGameAssets();
				CreateWorldAndSpawnPlayer();
				PlaceCamera(1.0f);
				CreateHud();
				RunFrames();
			}

		private:
			GameDefinition definition;
			std::filesystem::path assetDirectory;

			GameSettings settings;
			Window window;
			Renderer renderer;
			Input input;
			Camera camera;
			BlockRegistry blocks;
			BlockAssets assets;
			SoundPlayer sounds;
			std::unique_ptr<GameSession> session;
			std::unique_ptr<DebugOverlay> debug;
			std::unique_ptr<TargetOverlay> target;
			std::unique_ptr<Crosshair> crosshair;


			void LoadGameAssets()
			{
				assetDirectory = AssetDirectory();
				assets.Load(assetDirectory, blocks);
				assets.Upload(renderer);
				sounds.LoadEvents(assetDirectory);
			}

			void CreateWorldAndSpawnPlayer()
			{
				session = std::make_unique<GameSession>(definition, blocks);
			}

			void CreateHud()
			{
				debug = std::make_unique<DebugOverlay>(renderer, *session, session->GetWorld());
				target = std::make_unique<TargetOverlay>(renderer, camera, session->GetWorld(), blocks);
				crosshair = std::make_unique<Crosshair>(
					renderer, assetDirectory / "textures/gui/sprites/hud/crosshair.png");
			}

			void RunFrames()
			{
				Ticker ticker(SimulationTickSeconds);
				while (Window::ProcessMessages())
				{
					session->StreamWorld();
					UpdateLook();
					AdvanceSimulation(ticker);
					PlaceCamera(ticker.GetAlpha());
					PlaySounds();
					Render();
				}
			}

			void UpdateLook()
			{
				const auto look = input.ConsumeLook();
				camera.Rotate(look.x * settings.mouseSensitivity, look.y * settings.mouseSensitivity);
				session->SetLook(camera.GetYaw(), camera.GetPitch());
			}

			void AdvanceSimulation(Ticker& ticker)
			{
				ticker.Update();
				while (ticker.ConsumeTick())
				{
					session->Tick(input.ConsumeTick());
				}
			}

			void PlaceCamera(const float alpha)
			{
				const auto position = session->CameraPosition(alpha);
				camera.SetPosition(position.x, position.y, position.z);
			}

			void PlaySounds() const
			{
				for (const auto& cue : session->TakeSounds())
				{
					if (cue.event.empty())
					{
						sounds.Play(cue.variants, cue.volume, cue.pitch);
					}
					else
					{
						sounds.PlayEvent(cue.event, cue.volume, cue.pitch);
					}
				}
				sounds.Update();
			}

			void Render()
			{
				BeginScene();
				DrawTerrain();
				DrawHud();
				renderer.EndFrame();
			}

			void BeginScene()
			{
				const auto& world = session->GetWorld();
				renderer.BeginFrame(camera, world.GetChunkRadius() * Chunk::Width - 2.0f);
				renderer.Clear(0.3f, 0.6f, 0.9f);
			}

			void DrawTerrain()
			{
				const auto& mining = session->Mining();
				renderer.SetBlockCracking(
					mining ? mining->GetPosition() : BlockPosition{},
					mining ? assets.CrackTexture(mining->GetProgress()) : std::nullopt
				);
				debug->SetRenderedBlocks(renderer.DrawWorld(session->GetWorld()));
			}

			void DrawHud() const
			{
				renderer.DrawDebug(session->Debug().Lines());
				if (settings.showDebug) debug->Render();
				target->Render();
				crosshair->Render();
			}
	};
}

int RunGame(GameDefinition definition)
{
	try
	{
		Application application(std::move(definition));
		application.Run();
		return 0;
	}
	catch (const std::exception& error)
	{
		MessageBoxA(nullptr, error.what(), "Game failed",MB_OK | MB_ICONERROR);
		return 1;
	}
}
