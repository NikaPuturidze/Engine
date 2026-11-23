#pragma once

#include <cmath>

class FootstepTracker
{
public:
	bool Advance(const float horizontalDistance, const bool grounded)
	{
		if (horizontalDistance < 0.0f || !std::isfinite(horizontalDistance)) return false;
		// Count horizontal travel in the air too, but emit footsteps only on ground.
		// A pending step can play on landing even if horizontal movement has stopped.
		walkDistance += static_cast<double>(horizontalDistance) * 0.6;
		if (!grounded || walkDistance <= nextStep) return false;
		nextStep = std::floor(walkDistance) + 1.0;
		return true;
	}

	void Reset()
	{
		walkDistance = 0.0;
		nextStep = 1.0;
	}

private:
	double walkDistance = 0.0;
	double nextStep = 1.0;
};
