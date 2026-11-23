#pragma once
#include "api/input.h"
#include <optional>
#include "engine/platform/window.h"
#include "engine/platform/sprint_input.h"

class Input
{
	public:
		explicit Input(Window& window);
		~Input();
		Input(const Input&) = delete;
		Input& operator=(const Input&) = delete;
		InputActions ConsumeTick();
		Vec2 ConsumeLook() const;

	private:
		Window& window;
		bool cursorHidden = false, removeBlockRequested = false, breakingHeld = false;
		bool placeBlockRequested = false, jumping = false, jumpRequested = false, toggleModeRequested = false;
		bool movingForward = false, movingBackward = false, movingLeft = false, movingRight = false;
		SprintInput sprintInput;

		InputActions ReadActions() const;
		void ClearPressedActions();
		static std::optional<RECT> CursorBounds(const HWND window, const bool focused);
		void CaptureCursor(const RECT& bounds);
		void ReleaseCursor();
		void HandleKey(const WPARAM key, const bool pressed, const bool repeated);
		void ResetInput();


		void UpdateCursor(bool focused);
		static LRESULT CALLBACK SubClassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
};
