#pragma once

#include "../window.h"
#include "../player/player.h"
#include "../camera/camera.h"
#include "sprint_input.h"

class Camera;
class Player;

class PlayerController
{
	public:
		explicit PlayerController(Window& window, Player& player, Camera& camera);
		~PlayerController();

		PlayerController(const PlayerController&) = delete;
		PlayerController& operator=(const PlayerController&) = delete;

		void Update(float tickDuration, const World& world);
		void Spawn(const World& world);
		void UpdateLook() const;
		void UpdateCamera(float alpha) const;
		void UpdateInteraction(World& world);
		DirectX::XMFLOAT3 GetPosition() const { return player.GetPosition(); }
		float GetYaw() const;
		float GetHorizontalSpeed() const { return horizontalSpeed; }

	private:
		Window& window;
		Player& player;
		Camera& camera;

		DirectX::XMFLOAT3 previousPosition;
		float horizontalSpeed = 0.0f;

		void UpdateMovement(float tickDuration) const;
		void UpdateCursor(bool focused);
		bool cursorHidden = false;
		bool removeBlockRequested = false;
		bool jumping = false;
		bool jumpRequested = false;

		bool movingForward = false;
		SprintInput sprintInput;
		bool movingBackward = false;
		bool movingLeft = false;
		bool movingRight = false;

		static LRESULT CALLBACK SubClassProc(
			HWND hwnd,
			UINT uMsg,
			WPARAM wParam,
			LPARAM lParam,
			UINT_PTR uIdSubclass,
			DWORD_PTR dwRefData
		);
};
