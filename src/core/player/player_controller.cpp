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

	UpdateCursor(GetFocus() == window.GetHandle());

	const auto position = player.GetPosition();
	camera.SetPosition(position.x, position.y + Player::EyeHeight, position.z);
}

PlayerController::~PlayerController()
{
	RemoveWindowSubclass(window.GetHandle(), SubClassProc, 1);
	UpdateCursor(false);
}

void PlayerController::Spawn(const World& world)
{
	player.Spawn(world);
	previousPosition = player.GetPosition();
	UpdateCamera(1.0f);
}

void PlayerController::UpdateCursor(const bool focused)
{
	const HWND hwnd = window.GetHandle();
	RECT bounds{};

	if (focused && !IsIconic(hwnd) && GetClientRect(hwnd, &bounds) &&
		bounds.right > bounds.left && bounds.bottom > bounds.top)
	{
		POINT origin{.x = bounds.left, .y = bounds.top};
		POINT end{.x = bounds.right, .y = bounds.bottom};
		ClientToScreen(hwnd, &origin);
		ClientToScreen(hwnd, &end);
		bounds = {.left = origin.x, .top = origin.y, .right = end.x, .bottom = end.y};
		ClipCursor(&bounds);
		if (!cursorHidden)
		{
			ShowCursor(FALSE);
			cursorHidden = true;
		}
		SetCursor(nullptr);
	}
	else if (cursorHidden)
	{
		ClipCursor(nullptr);
		ShowCursor(TRUE);
		SetCursor(LoadCursor(nullptr, IDC_ARROW));
		cursorHidden = false;
	}
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
		case WM_SETFOCUS:
			controller->UpdateCursor(true);
			break;
		case WM_MOVE:
		case WM_SIZE:
		case WM_EXITSIZEMOVE:
			controller->UpdateCursor(GetFocus() == hwnd);
			break;
		case WM_ENTERSIZEMOVE:
			controller->UpdateCursor(false);
			break;
		case WM_SETCURSOR:
			if (controller->cursorHidden && LOWORD(lParam) == HTCLIENT)
			{
				SetCursor(nullptr);
				return TRUE;
			}
			break;
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
			controller->UpdateCursor(false);
			controller->movingForward = false;
			controller->movingBackward = false;
			controller->movingLeft = false;
			controller->movingRight = false;
			break;
		case WM_NCDESTROY:
			controller->UpdateCursor(false);
			RemoveWindowSubclass(hwnd, SubClassProc, uIdSubclass);
			break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
