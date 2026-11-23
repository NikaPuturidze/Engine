#pragma once

#include "../window.h"
#include "../player/player.h"
#include "../camera/camera.h"
#include "sprint_input.h"
#include "footstep_tracker.h"
#include "../../world/block.h"

class BlockRegistry;
class SoundPlayer;

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
		void UpdateInteraction(World& world, const BlockRegistry& blocks, SoundPlayer& sounds,
			BlockId placementBlock, float deltaSeconds);
		void UpdateFootsteps(float deltaSeconds, const World& world, const BlockRegistry& blocks, SoundPlayer& sounds);
		void UpdateLandingSounds(const World& world, const BlockRegistry& blocks, SoundPlayer& sounds);
		const std::optional<Block>& GetBreakingBlock() const { return breakingBlock; }
		DirectX::XMFLOAT3 GetPosition() const { return player.GetPosition(); }
		float GetYaw() const;
		float GetHorizontalSpeed() const { return horizontalSpeed; }
		GameMode GetGameMode() const { return player.GetGameMode(); }

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
		bool breakingHeld = false;
		std::optional<Block> breakingBlock;
		float hitSoundTime = 0.0f;
		float creativeBreakCooldown = 0.0f;
		GameMode previousGameMode = GameMode::Survival;
		FootstepTracker footsteps;
		bool placeBlockRequested = false;
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
