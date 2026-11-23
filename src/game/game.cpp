#include "api/game.h"
#include "game/settings.h"

void GenerateTerrain(const Terrain& terrain);

static void UpdateGame(const Game& game, const InputActions& input)
{
	game.player.SetMoveInput(input.move, input.sprinting);
	if (input.toggleModePressed) game.player.ToggleMode();
	if (input.jumpHeld || input.jumpPressed) game.player.RequestJump();
	game.player.SetMining(input.mineHeld, input.minePressed);
	if (input.placePressed) game.player.TryPlaceSelectedBlock();
}

int main()
{
	return RunGame(
		{
			.settings = CreateSettings(),
			.generateTerrain = GenerateTerrain,
			.update = UpdateGame
		}
	);
}
