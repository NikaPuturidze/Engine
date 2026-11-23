#pragma once

#include <chrono>
#include <optional>

class SprintInput
{
public:
    using Clock = std::chrono::steady_clock;

    void SetForward(bool pressed, Clock::time_point now = Clock::now())
    {
        if (pressed == forwardHeld) return; // Ignore keyboard auto-repeat.
        forwardHeld = pressed;
        if (!pressed)
        {
            sprinting = false;
            return;
        }
        sprinting = lastPress && now - *lastPress <= std::chrono::milliseconds(350);
        if (sprinting) lastPress.reset();
        else lastPress = now;
    }

    void Cancel()
    {
        sprinting = false;
        lastPress.reset();
    }

    void Reset()
    {
        Cancel();
        forwardHeld = false;
    }

    bool IsSprinting() const { return sprinting; }

private:
    bool forwardHeld = false;
    bool sprinting = false;
    std::optional<Clock::time_point> lastPress;
};
