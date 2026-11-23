#pragma once

#include "../graphics/cube.h"
#include "../graphics/texture.h"
#include "../core/game_mode.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct BlockPosition
{
	int x = 0, y = 0, z = 0;
	bool operator==(const BlockPosition&) const = default;
};

struct BlockDefinition
{
	BlockId id = 0;
	bool isDestructible = true;
	bool isOpenable = false;
	bool requiresToolForDrops = false;
	TextureId sideTexture = 0;
	TextureId topTexture = 0;
	TextureId bottomTexture = 0;
	std::array<TextureId, 6> faceTextures{};
	std::vector<std::string> movementSounds;
	std::vector<std::string> breakingSounds;
	std::vector<std::string> placingSounds;
	std::vector<std::string> hittingSounds;
	std::vector<std::string> fallingSounds;
	float maxDurability = 1.0f; // Block hardness, in Minecraft hardness units.
	std::vector<TextureId> crackingTextures;
};

class Block
{
	public:
		Block(std::shared_ptr<
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
		void SetPosition(BlockPosition value) { position = value; }
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

		bool UpdateMining(GameMode mode, float deltaSeconds, bool onGround = true)
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

		std::optional<TextureId> GetCrackingTexture() const
		{
			const auto& textures = definition->crackingTextures;
			if (textures.empty() || IsBroken() || durability == definition->maxDurability)
				return std::nullopt;
			const double progress = 1.0 - static_cast<double>(durability) / definition->maxDurability;
			const auto stage = (std::min)(static_cast<std::size_t>(progress * textures.size()), textures.size() - 1);
			return textures[stage];
		}

	private:
		std::shared_ptr<const BlockDefinition> definition;
		BlockPosition position;
		double durability;
		double handTickRemainder = 0.0;
};
