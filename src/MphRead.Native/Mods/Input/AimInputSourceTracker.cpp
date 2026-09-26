#include "AimInputSourceTracker.hpp"

#include <cmath>

namespace MphRead::Mods::Input
{
    const char* ToString(AimInputSource value) noexcept
    {
        switch (value)
        {
        case AimInputSource::None: return "None";
        case AimInputSource::Mouse: return "Mouse";
        case AimInputSource::Touch: return "Touch";
        case AimInputSource::Gamepad: return "Gamepad";
        }
        return "";
    }

    void AimInputSourceTracker::Pointer(float x, float y, bool touch, std::int64_t milliseconds) noexcept
    {
        static_cast<void>(milliseconds);
        if (!std::isfinite(x) || !std::isfinite(y) || x * x + y * y < .0001F)
        {
            return;
        }
        _current = touch ? AimInputSource::Touch : AimInputSource::Mouse;
        _claimStart = -1;
    }

    void AimInputSourceTracker::Stick(float x, float y, std::int64_t milliseconds) noexcept
    {
        if (!std::isfinite(x) || !std::isfinite(y) || x * x + y * y <= .08F * .08F)
        {
            _claimStart = -1;
            return;
        }
        if (_current == AimInputSource::Gamepad)
        {
            return;
        }
        if (_claimStart < 0)
        {
            _claimStart = milliseconds;
        }
        if (_current == AimInputSource::None || milliseconds - _claimStart >= 120)
        {
            _current = AimInputSource::Gamepad;
        }
    }

    void AimInputSourceTracker::Reset() noexcept
    {
        _current = AimInputSource::None;
        _claimStart = -1;
        _revision++;
    }
}
