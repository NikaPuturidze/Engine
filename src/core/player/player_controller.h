#pragma once

#include "../window.h"
#include "../player/player.h"
#include "../camera/camera.h"

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
		void UpdateLook() const;
		void UpdateCamera(float alpha) const;
		DirectX::XMFLOAT3 GetPosition() const { return player.GetPosition(); }

	private:
		Window& window;
		Player& player;
		Camera& camera;

		DirectX::XMFLOAT3 previousPosition;

		void UpdateMovement(float tickDuration) const;

		bool movingForward = false;
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
