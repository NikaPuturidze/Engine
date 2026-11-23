#pragma once

#include <DirectXMath.h>

class World;

class Entity
{
	protected:
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 velocity{0.0f, 0.0f, 0.0f};

		static constexpr float TicksPerSecond = 20.0f;
		// Gravity in blocks per tick squared; velocity is stored in blocks per second.
		static constexpr float GravityAcceleration = 0.08f;
		static constexpr float VerticalDragPerTick = 0.98f;
		static constexpr float HorizontalDragPerTick = 0.91f;
		static constexpr float GroundFrictionMultiplier = 0.91f * 0.6f;

		void ApplyMovementInput(float right, float forward, float yawDegrees, float acceleration, float deltaTime);
		void UpdatePhysics(float deltaTime, const World& world);
		void MoveAlongAxis(int axis, float distance, const World& world);
		static constexpr float HalfWidth = 0.3f;
		static constexpr float Height = 1.8f;

	public:
		explicit Entity(const DirectX::XMFLOAT3& startPos) : position(startPos) {}
		virtual ~Entity() = default;

		virtual void Update(float deltaTime, const World& world) = 0;

		DirectX::XMFLOAT3 GetPosition() const { return position; }
};
