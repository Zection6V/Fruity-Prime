#include "WeaponSelectionDirection.hpp"

#include "GamepadOptions.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace MphRead::Mods::Input
{
    namespace
    {
        constexpr float Pi = std::numbers::pi_v<float>;
    }

    std::int32_t WeaponSelectionDirection::Resolve(float x, float y) noexcept
    {
        if (x < 0 || y < 0 || x * x + y * y < .04F)
        {
            return -1;
        }
        const float angle = std::atan2(x, y);
        return std::clamp(static_cast<std::int32_t>(angle / (Pi / 12)), 0, 5);
    }

    std::int32_t WeaponSelectionDirection::ControllerSlot(float x, float y)
    {
        const float threshold = GamepadOptions::WheelThreshold();
        if (x * x + y * y < threshold * threshold)
        {
            return -1;
        }
        float angle = std::atan2(x, y);
        if (angle < 0)
        {
            angle += 2 * Pi;
        }
        return GamepadOptions::WheelOrder()[static_cast<std::size_t>(std::clamp(static_cast<std::int32_t>(angle / (Pi / 3)), 0, 5))];
    }

    std::pair<float, float> WeaponSelectionDirection::FromStick(float x, float y) noexcept
    {
        if (x * x + y * y < .20F)
        {
            return {0.0F, 0.0F};
        }
        float angle = std::atan2(x, y);
        if (angle < 0)
        {
            angle += 2 * Pi;
        }
        angle /= 4;
        return {std::sin(angle), std::cos(angle)};
    }
}
