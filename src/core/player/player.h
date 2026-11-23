#pragma once

#include <DirectXMath.h>
#include "../entity/entity.h"

class Player : public Entity
{
	public:
		explicit Player(const DirectX::XMFLOAT3 position);

		void Move(float right, float forward, float yawDegrees, float deltaTime);
		void Update(float deltaTime, const World& world) override;
		static constexpr float EyeHeight = 1.62f;

	private:
		static constexpr float WalkAcceleration = 0.1f;
		static constexpr float AirAcceleration = 0.02f;
};
