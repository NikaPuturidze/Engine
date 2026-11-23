#include "engine/platform/input.h"
#include <commctrl.h>
#include <stdexcept>
Input::Input(Window& window) : window(window)
{
    if (!SetWindowSubclass(window.GetHandle(),SubClassProc,1,reinterpret_cast<DWORD_PTR>(this)))
        throw std::runtime_error("Failed to install input handler");
    UpdateCursor(GetFocus()==window.GetHandle());
}
Input::~Input()
{
    RemoveWindowSubclass(window.GetHandle(),SubClassProc,1);
    UpdateCursor(false);
}
InputActions Input::ConsumeTick()
{
    InputActions result;
    result.move={.x = float(movingRight)-float(movingLeft),.y = float(movingForward)-float(movingBackward)};
    result.sprinting=sprintInput.IsSprinting() && !movingBackward;
    result.jumpHeld=jumping; result.jumpPressed=jumpRequested;
    result.mineHeld=breakingHeld; result.minePressed=removeBlockRequested;
    result.placePressed=placeBlockRequested; result.toggleModePressed=toggleModeRequested;
    jumpRequested=removeBlockRequested=placeBlockRequested=toggleModeRequested=false;
    return result;
}
Vec2 Input::ConsumeLook() const
{
    long x=0,y=0; window.ConsumeMouseMovement(x,y);
    return {.x = float(x),.y = float(y)};
}
void Input::UpdateCursor(const bool focused)
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
				const bool pressed = uMsg == WM_KEYDOWN;
				if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL)
					controller->sprintInput.SetSprintKey(pressed);
				if (wParam == VK_F4 && pressed && !(lParam & (1LL << 30)))
					controller->toggleModeRequested = true;
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
			controller->breakingHeld = false;
			controller->placeBlockRequested = false;
			controller->jumping = false;
			controller->jumpRequested = false;
			controller->toggleModeRequested = false;
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
