#include "MorphBallTouchRules.hpp"

#include <cmath>

namespace MphRead::Entities::MorphBallTouchRules
{
    BoostBranch Arbitrate(bool boosting, bool canTouchBoost, bool continued,
        std::int16_t delta4X, std::int16_t delta4Y) noexcept
    {
        if (boosting || !canTouchBoost || !continued)
        {
            return BoostBranch::Shoulder;
        }
        const std::int32_t dx = delta4X;
        const std::int32_t dy = delta4Y;
        return dx * dx + dy * dy > TouchBoostThresholdSquared ? BoostBranch::TouchBoost : BoostBranch::SkipShoulder;
    }

    PlanarDelta TouchRoll(std::int16_t delta4X, std::int16_t delta4Y, float scale,
        float rollFbX, float rollFbZ, float rollLrX, float rollLrZ) noexcept
    {
        const float dx = static_cast<float>(delta4X);
        const float dy = static_cast<float>(delta4Y);
        return {
            -dy * scale * rollFbX - dx * scale * rollLrX,
            -dy * scale * rollFbZ - dx * scale * rollLrZ
        };
    }

    PlanarDelta TouchRollStep(std::int16_t delta4X, std::int16_t delta4Y, float scale,
        float rollFbX, float rollFbZ, float rollLrX, float rollLrZ, float share) noexcept
    {
        const auto native = TouchRoll(delta4X, delta4Y, scale, rollFbX, rollFbZ, rollLrX, rollLrZ);
        return {native.X * share, native.Z * share};
    }

    PlanarDelta TouchBoostImpulse(std::int32_t dx, std::int32_t dy, float boostSpeedMax,
        float cameraForwardX, float cameraForwardZ, float cameraSideX, float cameraSideZ) noexcept
    {
        const float dxF = static_cast<float>(dx);
        const float dyF = static_cast<float>(dy);
        const float mag = std::sqrt(dxF * dxF + dyF * dyF);
        if (!(mag > 0.0F))
        {
            return {};
        }
        const float side = -dxF / mag * boostSpeedMax;
        const float forward = -dyF / mag * boostSpeedMax;
        return {cameraForwardX * forward + cameraSideX * side, cameraForwardZ * forward + cameraSideZ * side};
    }
}
