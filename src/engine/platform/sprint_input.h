#pragma once

#include <chrono>
#include <optional>

class SprintInput
{
public:
    using Clock = std::chrono::steady_clock;

    void SetForward(const bool pressed, Clock::time_point now = Clock::now())
    {
        if (pressed == forwardHeld) return; // Ignore keyboard auto-repeat.
        forwardHeld = pressed;
        if (!pressed)
        {
            sprinting = false;
            return;
        }
        sprinting = sprintKeyHeld || (lastPress && now - *lastPress <= std::chrono::milliseconds(350));
        if (sprinting) lastPress.reset();
        else lastPress = now;
    }

    // Ctrl starts sprinting while moving forward. Releasing it does not cancel an
    // active sprint; releasing forward, moving backward or losing focus does.
    void SetSprintKey(bool pressed)
    {
        if (pressed == sprintKeyHeld) return;
        sprintKeyHeld = pressed;
        if (pressed && forwardHeld) sprinting = true;
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
        sprintKeyHeld = false;
    }

    bool IsSprinting() const { return sprinting; }

private:
    bool forwardHeld = false;
    bool sprintKeyHeld = false;
    bool sprinting = false;
    std::optional<Clock::time_point> lastPress;
};
