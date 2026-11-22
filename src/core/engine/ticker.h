#pragma once

#include <chrono>

class Ticker
{
	public:
		explicit Ticker(double tickDuration);

		void Update();
		bool ConsumeTick();
		float GetAlpha() const;

	private:
		double tickDuration;
		double accumulator = 0.0;
		std::chrono::steady_clock::time_point previous;
};
