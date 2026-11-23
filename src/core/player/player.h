#pragma once

#include <DirectXMath.h>
#include "../entity/entity.h"

class Player : public Entity
{
	public:
		explicit Player(const DirectX::XMFLOAT3 position);

		void Move(float right, float forward, float yawDegrees, float deltaTime, bool sprint = false);
		void Update(float deltaTime, const World& world) override;
		void Spawn(const World& world);
		void Jump();
		static constexpr float EyeHeight = 1.62f;

	private:
		bool sprinting = false;
		float movementYaw = 0.0f;
		static constexpr float SprintMultiplier = 1.3f;
		static constexpr float SprintJumpBoostPerTick = 0.2f;
		static constexpr float JumpVelocityPerTick = 0.42f;
		static constexpr float WalkAcceleration = 0.1f;
		static constexpr float AirAcceleration = 0.02f;
};
