#include "ticker.h"
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
	accumulator += std::chrono::duration<double>(now - previous).count();
	previous = now;
}

bool Ticker::ConsumeTick()
{
	if (accumulator < tickDuration)
		return false;

	accumulator -= tickDuration;
	return true;
}

float Ticker::GetAlpha() const
{
	return static_cast<float>(accumulator / tickDuration);
}
