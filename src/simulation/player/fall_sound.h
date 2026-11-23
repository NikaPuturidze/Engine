#pragma once

#include "api/game_mode.h"
#include <cmath>

enum class FallSound
{
	None,
	Small,
	Big
};

inline FallSound GetFallSound(const float fallDistance, const GameMode mode)
{
	if (mode == GameMode::Creative || !std::isfinite(fallDistance) || fallDistance <= 3.0f)
		return FallSound::None;
	// Vanilla Java: ceil(distance - 3) damage; big sound above 4 damage points.
	return std::ceil(fallDistance - 3.0f) > 4 ? FallSound::Big : FallSound::Small;
}
