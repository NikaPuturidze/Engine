#include "player_controller.h"
#include "../../world/block_raycast.h"
#include <commctrl.h>
#include <cmath>
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
	horizontalSpeed = 0.0f;
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
	if (jumping || jumpRequested) player.Jump();
	jumpRequested = false;
	player.Update(tickDuration, world);
	const auto position = player.GetPosition();
	const float dx = position.x - previousPosition.x;
	const float dz = position.z - previousPosition.z;
	horizontalSpeed = tickDuration > 0.0f ? std::hypot(dx, dz) / tickDuration : 0.0f;
}

float PlayerController::GetYaw() const
{
	const float yaw = std::fmod(camera.GetYaw(), 360.0f);
	return yaw < 0.0f ? yaw + 360.0f : yaw;
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

void PlayerController::UpdateInteraction(World& world)
{
	if (!removeBlockRequested) return;
	removeBlockRequested = false;
	DirectX::XMFLOAT3 forward;
	const auto inverseView = DirectX::XMMatrixInverse(nullptr, camera.GetViewMatrix());
	DirectX::XMStoreFloat3(&forward, DirectX::XMVector3TransformNormal(
		DirectX::XMVectorSet(0, 0, 1, 0), inverseView));
	if (const auto hit = RaycastBlock(world, camera.GetPosition(), forward,
		world.GetChunkRadius() * Chunk::Width - 2.0f))
		world.SetBlock(hit->x, hit->y, hit->z, 0);
}

void PlayerController::UpdateMovement(const float tickDuration) const
{
	player.Move(
		static_cast<float>(movingRight) - static_cast<float>(movingLeft),
		static_cast<float>(movingForward) - static_cast<float>(movingBackward),
		camera.GetYaw(),
		tickDuration,
		sprintInput.IsSprinting() && !movingBackward
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
		case WM_LBUTTONDOWN:
			if (GetFocus() == hwnd && controller->cursorHidden)
				controller->removeBlockRequested = true;
			break;
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
				if (wParam == 'W')
				{
					controller->movingForward = pressed;
					controller->sprintInput.SetForward(pressed);
				}
				if (wParam == 'S')
				{
					controller->movingBackward = pressed;
					if (pressed) controller->sprintInput.Cancel();
				}
				if (wParam == 'A') controller->movingLeft = pressed;
				if (wParam == 'D') controller->movingRight = pressed;
				if (wParam == VK_SPACE)
				{
					if (pressed && !controller->jumping) controller->jumpRequested = true;
					controller->jumping = pressed;
				}
				break;
			}
		case WM_KILLFOCUS:
			controller->sprintInput.Reset();
			controller->removeBlockRequested = false;
			controller->jumping = false;
			controller->jumpRequested = false;
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
