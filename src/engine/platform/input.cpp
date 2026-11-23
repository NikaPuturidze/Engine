#include "engine/platform/input.h"
#include <commctrl.h>
#include <stdexcept>

Input::Input(Window& window) : window(window)
{
	if (!SetWindowSubclass(window.GetHandle(), SubClassProc, 1, reinterpret_cast<DWORD_PTR>(this)))
		throw std::runtime_error("Failed to install input handler");
	UpdateCursor(GetFocus() == window.GetHandle());
}

Input::~Input()
{
	RemoveWindowSubclass(window.GetHandle(), SubClassProc, 1);
	UpdateCursor(false);
}

InputActions Input::ConsumeTick()
{
	const auto result = ReadActions();
	ClearPressedActions();
	return result;
}

InputActions Input::ReadActions() const
{
	InputActions result;
	result.move = {
		.x = static_cast<float>(movingRight) - static_cast<float>(movingLeft),
		.y = static_cast<float>(movingForward) - static_cast<float>(movingBackward)
	};
	result.sprinting = sprintInput.IsSprinting() && !movingBackward;
	result.jumpHeld = jumping;
	result.jumpPressed = jumpRequested;
	result.mineHeld = breakingHeld;
	result.minePressed = removeBlockRequested;
	result.placePressed = placeBlockRequested;
	result.toggleModePressed = toggleModeRequested;

	return result;
}

void Input::ClearPressedActions()
{
	jumpRequested = false;
	removeBlockRequested = false;
	placeBlockRequested = false;
	toggleModeRequested = false;
}

Vec2 Input::ConsumeLook() const
{
	long x = 0, y = 0;
	window.ConsumeMouseMovement(x, y);
	return {
		.x = static_cast<float>(x),
		.y = static_cast<float>(y)
	};
}

void Input::UpdateCursor(const bool focused)
{
	const auto bounds = CursorBounds(window.GetHandle(), focused);
	if (bounds) CaptureCursor(*bounds);
	else ReleaseCursor();
}

std::optional<RECT> Input::CursorBounds(const HWND window, const bool focused)
{
	RECT bounds{};
	if (!focused || IsIconic(window) || !GetClientRect(window, &bounds) || bounds.right <= bounds.left || bounds.bottom
		<= bounds.top)
		return std::nullopt;
	POINT origin{bounds.left, bounds.top};
	POINT end{bounds.right, bounds.bottom};
	ClientToScreen(window, &origin);
	ClientToScreen(window, &end);
	return RECT{origin.x, origin.y, end.x, end.y};
}

void Input::CaptureCursor(const RECT& bounds)
{
	ClipCursor(&bounds);
	if (!cursorHidden)
	{
		ShowCursor(FALSE);
		cursorHidden = true;
	}
	SetCursor(nullptr);
}

void Input::ReleaseCursor()
{
	if (!cursorHidden) return;
	ClipCursor(nullptr);
	ShowCursor(TRUE);
	SetCursor(LoadCursor(nullptr, IDC_ARROW));
	cursorHidden = false;
}

LRESULT CALLBACK Input::SubClassProc(
	const HWND hwnd,
	const UINT uMsg,
	const WPARAM wParam,
	const LPARAM lParam,
	const UINT_PTR uIdSubclass,
	const DWORD_PTR dwRefData)
{
	auto* controller = reinterpret_cast<Input*>(dwRefData);

	switch (uMsg)
	{
		case WM_RBUTTONDOWN:
			if (GetFocus() == hwnd && controller->cursorHidden)
				controller->placeBlockRequested = true;
			break;
		case WM_LBUTTONDOWN:
			if (GetFocus() == hwnd && controller->cursorHidden)
			{
				controller->removeBlockRequested = true;
				controller->breakingHeld = true;
			}
			break;
		case WM_LBUTTONUP:
			controller->breakingHeld = false;
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
				controller->HandleKey(wParam, uMsg == WM_KEYDOWN, (lParam & (1LL << 30)) != 0);
				break;
			}
		case WM_KILLFOCUS:
			controller->ResetInput();
			break;
		case WM_NCDESTROY:
			controller->UpdateCursor(false);
			RemoveWindowSubclass(hwnd, SubClassProc, uIdSubclass);
			break;
	}
	return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void Input::HandleKey(const WPARAM key, const bool pressed, const bool repeated)
{
	if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL)
		sprintInput.SetSprintKey(pressed);
	if (key == VK_F4 && pressed && !repeated)
		toggleModeRequested = true;
	if (key == 'W')
	{
		movingForward = pressed;
		sprintInput.SetForward(pressed);
	}
	if (key == 'S')
	{
		movingBackward = pressed;
		if (pressed) sprintInput.Cancel();
	}
	if (key == 'A') movingLeft = pressed;
	if (key == 'D') movingRight = pressed;
	if (key == VK_SPACE)
	{
		if (pressed && !jumping) jumpRequested = true;
		jumping = pressed;
	}
}

void Input::ResetInput()
{
	sprintInput.Reset();
	removeBlockRequested = false;
	breakingHeld = false;
	placeBlockRequested = false;
	jumping = false;
	jumpRequested = false;
	toggleModeRequested = false;
	UpdateCursor(false);
	movingForward = false;
	movingBackward = false;
	movingLeft = false;
	movingRight = false;
}
