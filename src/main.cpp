#include "core/window.h"
#include "core/player/player.h"
#include "core/camera/camera.h"
#include "core/engine/engine.h"
#include "core/player/player_controller.h"
#include "graphics/renderer.h"

int main()
{
	Window window;
	Renderer renderer(window);
	Player player({0, 100.0f, 0});
	Camera camera;
	PlayerController playerController(window, player, camera);

	const Engine engine(window, renderer, playerController, camera);

	engine.Run();

	return 0;
}
