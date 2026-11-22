#include "core/window.h"
#include "graphics/renderer.h"
#include "world/block_registry.h"

int main()
{
	Window window;
	Renderer renderer(window.GetHandle());

	BlockRegistry blocks(renderer);
	blocks.LoadNamespace(
		std::filesystem::path(ENGINE_ASSET_DIRECTORY) / "default",
		"default"
	);

	const Cube cube(blocks.GetBlockId("default:block/stone"));
	const Cube cube2(blocks.GetBlockId("default:block/grass"));

	cube2.x = 2.0f;

	Camera camera;

    camera.SetPosition(0.0f, 4.0f, -3.0f);

    while (window.ProcessMessages())
    {
		camera.Update(window);

		renderer.BeginFrame(camera);

		renderer.Clear(
			0.0f / 255.0f,
			0.0f / 255.0f,
			0.0f / 255.0f
		);

		renderer.DrawCube(cube);
		renderer.DrawCube(cube2);

		renderer.EndFrame();
	}

	return 0;
}
