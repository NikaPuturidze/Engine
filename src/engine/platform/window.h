#pragma once

#include <string>
#include <windows.h>

class Window
{
	public:
		Window();
		~Window();
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;
		static bool ProcessMessages();
		HWND GetHandle() const;
		void ConsumeMouseMovement(long& x, long& y);
		void AddMouseMovement(long x, long y);
		std::wstring text;

	private:
		HWND hwnd = nullptr;
		long mouseX = 0;
		long mouseY = 0;
};
