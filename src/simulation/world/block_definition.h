#pragma once
#include "api/types.h"
#include <memory>
#include <string>
#include <vector>

struct BlockDefinition
{
	BlockId id = 0;
	bool isDestructible = true;
	bool isOpenable = false;
	bool requiresToolForDrops = false;
	std::vector<std::string> movementSounds;
	std::vector<std::string> breakingSounds;
	std::vector<std::string> placingSounds;
	std::vector<std::string> hittingSounds;
	std::vector<std::string> fallingSounds;
	float maxDurability = 1.0f; // Block hardness, in Minecraft hardness units.
};
