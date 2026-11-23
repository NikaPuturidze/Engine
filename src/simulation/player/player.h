#pragma once

#include "api/math.h"
#include "simulation/physics/entity.h"
#include "api/game_mode.h"
#include "simulation/player/fall_sound.h"
#include "api/types.h"

class Player : public Entity
{
	public:
		explicit Player(const Vec3 position);

		void Move(float right, float forward, float yawDegrees, float deltaTime, bool sprint = false, float speedMultiplier = 1.0f);
		void Update(float deltaTime, const World& world) override;
		void Spawn(const World& world);
		void Jump();
		GameMode GetGameMode() const { return gameMode; }
		void SetGameMode(const GameMode mode)
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

        float FindSpawnHeight(const World& world) const;
        void ResetFallTracking();
        bool IntersectsBlock(const BlockPosition block) const;
        void TrackFallAndLanding(const World& world, const float previousY, const bool wasGrounded);
        void RecordLanding(const World& world);
        BlockId FindSupportingBlock(const World& world) const;


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
