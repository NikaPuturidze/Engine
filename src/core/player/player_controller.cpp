#include "player_controller.h"
#include <commctrl.h>
#include <stdexcept>

PlayerController::PlayerController(
	Window& window, Player& player, Camera& camera) :
	window(window), player(player), camera(camera), previousPosition(player.GetPosition())
{
	if (!SetWindowSubclass(window.GetHandle(), SubClassProc, 1,
	                       reinterpret_cast<DWORD_PTR>(this)))
		throw std::runtime_error("Failed to install player input handler");

	ShowCursor(FALSE);

	const auto position = player.GetPosition();
	camera.SetPosition(position.x, position.y + Player::EyeHeight, position.z);
}

PlayerController::~PlayerController()
{
	RemoveWindowSubclass(window.GetHandle(), SubClassProc, 1);
	ShowCursor(TRUE);
}

void PlayerController::Update(const float tickDuration, const World& world)
{
	previousPosition = player.GetPosition();
	UpdateMovement(tickDuration);
	player.Update(tickDuration, world);
}

void PlayerController::UpdateCamera(const float alpha) const
{
	const auto position = player.GetPosition();
	camera.SetPosition(
		previousPosition.x + (position.x - previousPosition.x) * alpha,
		previousPosition.y + (position.y - previousPosition.y) * alpha + Player::EyeHeight,
		previousPosition.z + (position.z - previousPosition.z) * alpha
	);
}

void PlayerController::UpdateLook() const
{
	long x = 0;
	long y = 0;
	window.ConsumeMouseMovement(x, y);
	constexpr float sensitivity = 0.1f;
	camera.Rotate(x * sensitivity, y * sensitivity);
}

void PlayerController::UpdateMovement(const float tickDuration) const
{
	player.Move(
		static_cast<float>(movingRight) - static_cast<float>(movingLeft),
		static_cast<float>(movingForward) - static_cast<float>(movingBackward),
		camera.GetYaw(),
		tickDuration
	);
}

LRESULT CALLBACK PlayerController::SubClassProc(
	const HWND hwnd,
	const UINT uMsg,
	const WPARAM wParam,
	const LPARAM lParam,
	const UINT_PTR uIdSubclass,
	const DWORD_PTR dwRefData)
{
	auto* controller = reinterpret_cast<PlayerController*>(dwRefData);

	switch (uMsg)
	{
		case WM_KEYDOWN:
		case WM_KEYUP:
			{
				const bool pressed = uMsg == WM_KEYDOWN;
				if (wParam == 'W') controller->movingForward = pressed;
				if (wParam == 'S') controller->movingBackward = pressed;
				if (wParam == 'A') controller->movingLeft = pressed;
				if (wParam == 'D') controller->movingRight = pressed;
				break;
			}
		case WM_KILLFOCUS:
			controller->movingForward = false;
			controller->movingBackward = false;
			controller->movingLeft = false;
			controller->movingRight = false;
			break;
		case WM_NCDESTROY:
			RemoveWindowSubclass(hwnd, SubClassProc, uIdSubclass);
			break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
