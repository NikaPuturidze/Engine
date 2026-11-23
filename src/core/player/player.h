#pragma once

#include <DirectXMath.h>
#include "../entity/entity.h"
#include "../game_mode.h"
#include "fall_sound.h"
#include "../../graphics/cube.h"

class Player : public Entity
{
	public:
		explicit Player(const DirectX::XMFLOAT3 position);

		void Move(float right, float forward, float yawDegrees, float deltaTime, bool sprint = false);
		void Update(float deltaTime, const World& world) override;
		void Spawn(const World& world);
		void Jump();
		GameMode GetGameMode() const { return gameMode; }
		void SetGameMode(GameMode mode)
		{
			if (gameMode == mode) return;
			gameMode = mode;
			fallDistance = 0.0f;
			landingSound = FallSound::None;
			landingBlock = 0;
		}
		BlockId ConsumeLandingBlock()
		{
			const auto block = landingBlock;
			landingBlock = 0;
			return block;
		}
		FallSound ConsumeLandingSound()
		{
			const auto sound = landingSound;
			landingSound = FallSound::None;
			return sound;
		}
		bool PlaceBlock(World& world, int x, int y, int z, BlockId block) const;
		static constexpr float EyeHeight = 1.62f;

	private:
		GameMode gameMode = GameMode::Survival;
		float fallDistance = 0.0f;
		FallSound landingSound = FallSound::None;
		BlockId landingBlock = 0;
		bool sprinting = false;
		float movementYaw = 0.0f;
		static constexpr float SprintMultiplier = 1.3f;
		static constexpr float SprintJumpBoostPerTick = 0.2f;
		static constexpr float JumpVelocityPerTick = 0.42f;
		static constexpr float WalkAcceleration = 0.1f;
		static constexpr float AirAcceleration = 0.02f;
};
