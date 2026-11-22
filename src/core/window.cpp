#include "window.h"
#include <windows.h>
#include <vector>
#include <stdexcept>

static LRESULT CALLBACK WindowProc(
	HWND hwnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
);

Window::Window()
{
	const HINSTANCE hInstance = GetModuleHandleA(nullptr);

	const char* className = "Engine Class";
	const char* windowName = "Engine";

	const WNDCLASSA wc = {
		.lpfnWndProc = WindowProc,
		.hInstance = hInstance,
		.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
		.lpszClassName = className,
	};
	RegisterClassA(&wc);

	this->hwnd = CreateWindowA(
		className,
		windowName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr, nullptr, hInstance, this
	);

    const RAWINPUTDEVICE mouse = {
        .usUsagePage = 0x01,
        .usUsage = 0x02,
        .dwFlags = 0,
        .hwndTarget = this->hwnd,
    };
    if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse)))
        throw std::runtime_error("Failed to register mouse input");

	ShowWindow(this->hwnd, SW_SHOW);
}

bool Window::ProcessMessages()
{
	MSG msg = {};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return false;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return true;
}

HWND Window::GetHandle() const
{
	return hwnd;
}


void Window::ConsumeMouseMovement(long& x, long& y)
{
    x = mouseX;
    y = mouseY;
    mouseX = 0;
    mouseY = 0;
}

void Window::AddMouseMovement(long x, long y)
{
    mouseX += x;
    mouseY += y;
}


LRESULT CALLBACK WindowProc(
	const HWND hwnd,
	const UINT uMsg,
	const WPARAM wParam,
	const LPARAM lParam)
{
	if (uMsg == WM_NCCREATE)
	{
		auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);

		SetWindowLongPtrA(
			hwnd,
			GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(create->lpCreateParams)
		);
	}

	auto* window = reinterpret_cast<Window*>(
		GetWindowLongPtrA(hwnd, GWLP_USERDATA)
	);

	switch (uMsg)
	{
        case WM_KILLFOCUS:
            if (window)
            {
                long x, y;
                window->ConsumeMouseMovement(x, y);
            }
            return 0;

        case WM_INPUT:
        {
            UINT size = 0;
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size,
                sizeof(RAWINPUTHEADER)) != 0 || size == 0)
                return 0;
            std::vector<BYTE> data(size);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, data.data(), &size,
                sizeof(RAWINPUTHEADER)) == size)
            {
                const auto* input = reinterpret_cast<const RAWINPUT*>(data.data());
                if (window && input->header.dwType == RIM_TYPEMOUSE &&
                    !(input->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
                    window->AddMouseMovement(input->data.mouse.lLastX, input->data.mouse.lLastY);
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				const HDC hdc = BeginPaint(hwnd, &ps);

				if (window)
					TextOutW(
						hdc,
						0,
						0,
						window->text.c_str(),
						static_cast<int>(window->text.size()));

				EndPaint(hwnd, &ps);
				return 0;
			}

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}
