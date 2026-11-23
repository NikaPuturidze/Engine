#pragma once

#include "api/math.h"

class World;

class Entity
{
	protected:
		Vec3 position;
		Vec3 velocity{.x = 0.0f, .y = 0.0f, .z = 0.0f};
		bool grounded = false;

		static constexpr float TicksPerSecond = 20.0f;
		// Gravity in blocks per tick squared; velocity is stored in blocks per second.
		static constexpr float GravityAcceleration = 0.08f;
		static constexpr float VerticalDragPerTick = 0.98f;
		static constexpr float HorizontalDragPerTick = 0.91f;
		static constexpr float GroundFrictionMultiplier = 0.91f * 0.6f;

		void ApplyMovementInput(float right, float forward, float yawDegrees, float acceleration, float deltaTime);
		void UpdatePhysics(float deltaTime, const World& world);

        void MoveHorizontally(const float deltaTime, const World& world);
        void ApplyHorizontalDrag(const float deltaTime);
        void MoveVertically(const float deltaTime, const World& world);
        void ApplyGravity(const float deltaTime);
        void ApplyAxisMovement(const int axis, const float distance, const float allowed);


		void MoveAlongAxis(int axis, float distance, const World& world);
		static constexpr float HalfWidth = 0.3f;
		static constexpr float Height = 1.8f;

	public:
		explicit Entity(const Vec3& startPos) : position(startPos) {}
		virtual ~Entity() = default;

		virtual void Update(float deltaTime, const World& world) = 0;

		Vec3 GetPosition() const { return position; }
		Bounds GetBounds() const { return {.minimum = position-Vec3{.x = HalfWidth,.y = 0,.z = HalfWidth}, .maximum = position+Vec3{.x = HalfWidth,.y = Height,.z = HalfWidth}}; }
		bool IsGrounded() const { return grounded; }
};
