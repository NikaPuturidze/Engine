#include "engine/runtime/ticker.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

Ticker::Ticker(const double tickDuration) :
	tickDuration(tickDuration), previous(std::chrono::steady_clock::now())
{
	if (!std::isfinite(tickDuration) || tickDuration <= 0.0)
		throw std::invalid_argument("Tick duration must be positive and finite");
}

void Ticker::Update()
{
	const auto now = std::chrono::steady_clock::now();
	const double elapsed = std::chrono::duration<double>(now - previous).count();
	Advance(elapsed);
	previous = now;
}

bool Ticker::ConsumeTick()
{
	if (accumulator + 1e-12 < tickDuration)
		return false;

	accumulator = std::max(0.0, accumulator - tickDuration);
	return true;
}

float Ticker::GetAlpha() const
{
	return static_cast<float>(std::clamp(accumulator / tickDuration, 0.0, 1.0));
}

void Ticker::Advance(const double elapsedSeconds)
{
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0)
        throw std::invalid_argument("Elapsed time must be finite and non-negative");
    // Catch up at most five ticks; discard excess wall time after a long stall.
    accumulator = std::min(accumulator + elapsedSeconds, tickDuration * 5);
}
