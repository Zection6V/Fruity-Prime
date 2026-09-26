#pragma once

#include "../../../NativeRuntime/System/Numerics.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Input::AimAssist
{
    class AimAssistMath final
    {
    public:
        AimAssistMath() = delete;

        [[nodiscard]] static float Smooth(float a, float b, float value) noexcept
        {
            const float t = std::clamp((value - a) / (b - a), 0.0F, 1.0F);
            return t * t * (3 - 2 * t);
        }

        [[nodiscard]] static bool Finite(System::Numerics::Vector2 v) noexcept
        {
            return std::isfinite(v.X) && std::isfinite(v.Y);
        }

        [[nodiscard]] static float Opposition(float input, float error) noexcept
        {
            return input * error < 0 ? 1 - Smooth(.02F, .8F, std::abs(input)) : 1;
        }

        [[nodiscard]] static float Score(float angle, float cone, float distance, bool retained, float motion) noexcept
        {
            return .60F * (1 - std::clamp(angle / cone, 0.0F, 1.0F)) + (retained ? .15F : 0)
                + .10F * (1 - std::clamp(distance / 60, 0.0F, 1.0F)) + .10F + .05F * std::clamp(motion, 0.0F, 1.0F);
        }
    };
}
