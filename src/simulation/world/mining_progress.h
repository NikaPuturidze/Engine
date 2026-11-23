#pragma once

#include "simulation/world/block_definition.h"
#include "api/game_mode.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>



class MiningProgress
{
	public:
		MiningProgress(std::shared_ptr<
			const BlockDefinition> definition,
			const BlockPosition position): definition(std::move(definition)), position(position)
		{
			if (!this->definition || this->definition->id == 0)
				throw std::invalid_argument("Block needs a non-air definition");

			if (!std::isfinite(this->definition->maxDurability) || this->definition->maxDurability <= 0)
				throw std::invalid_argument("Block durability must be finite and positive");

			durability = this->definition->maxDurability;
		}

		BlockId GetId() const { return definition->id; }
		const BlockDefinition& GetDefinition() const { return *definition; }
		BlockPosition GetPosition() const { return position; }
		void SetPosition(const BlockPosition value) { position = value; }
		bool IsDestructible() const { return definition->isDestructible; }
		bool IsOpenable() const { return definition->isOpenable; }
		float GetDurability() const { return static_cast<float>(durability); }
		bool IsBroken() const { return durability == 0.0f; }

		bool Damage(const float amount)
		{
			if (!std::isfinite(amount) || amount < 0)
				throw std::invalid_argument("Block damage must be finite and non-negative");

			if (!IsDestructible() || IsBroken()) return false;

			durability = (std::max)(0.0, durability - amount);
			return IsBroken();
		}

		bool UpdateBreaking(const float damagePerSecond, const float deltaSeconds)
		{
			if (!std::isfinite(damagePerSecond) || damagePerSecond < 0 || !std::isfinite(deltaSeconds) || deltaSeconds < 0)
				throw std::invalid_argument("Breaking rate and time must be finite and non-negative");

			const double amount = static_cast<double>(damagePerSecond) * deltaSeconds;
			return Damage(static_cast<float>((std::min)(amount, static_cast<double>(definition->maxDurability))));
		}

		// Bare hands have mining speed 1. Minecraft applies progress at 20 ticks/s.
		bool UpdateHandBreaking(const float deltaSeconds, const bool onGround = true)
		{
			if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
				throw std::invalid_argument("Breaking time must be finite and non-negative");
			if (!IsDestructible() || IsBroken()) return false;
			const double elapsedTicks = handTickRemainder + static_cast<double>(deltaSeconds) * 20.0;
			const double ticks = std::floor(elapsedTicks + 1e-6);
			handTickRemainder = (std::max)(0.0, elapsedTicks - ticks);
			if (ticks == 0) return false;
			const double divisor = definition->requiresToolForDrops ? 100.0 : 30.0;
			const double damage = ticks / (divisor * (onGround ? 1.0 : 5.0));
			// Avoid an extra tick caused by float representations of values like 0.6.
			if (damage >= durability - definition->maxDurability * 1e-6)
			{
				durability = 0.0;
				return true;
			}
			return Damage(static_cast<float>(damage));
		}

		bool UpdateMining(const GameMode mode, const float deltaSeconds, const bool onGround = true)
		{
			if (mode == GameMode::Survival) return UpdateHandBreaking(deltaSeconds, onGround);
			if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
				throw std::invalid_argument("Breaking time must be finite and non-negative");
			if (IsBroken()) return false;
			// Creative can also remove blocks that are unbreakable in Survival.
			durability = 0.0;
			return true;
		}

		void ResetDurability()
		{
			durability = definition->maxDurability;
			handTickRemainder = 0.0;
		}

		float GetProgress() const
		{
			return static_cast<float>(1.0 - durability / definition->maxDurability);
		}

	private:
		std::shared_ptr<const BlockDefinition> definition;
		BlockPosition position;
		double durability;
		double handTickRemainder = 0.0;
};
