#include "core/window.h"
#include "graphics/renderer.h"

int main()
{
	Window window;
	Renderer renderer(window.GetHandle());

	const Cube cube(1);
	const Cube cube2(2);
	cube2.x = 2.0f;

	Camera camera;

    camera.SetPosition(3.0f, 2.0f, -5.0f);

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
