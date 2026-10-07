#include "NativeTouchState.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Input
{
    std::int16_t Sum4AsS16(const std::array<std::int16_t, 4>& samples) noexcept
    {
        const std::int32_t sum = static_cast<std::int32_t>(samples[0]) + samples[1] + samples[2] + samples[3];
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(sum) & 0xFFFFU));
    }

    void NativeTouchState::Clear() noexcept
    {
        *this = NativeTouchState{};
    }

    void NativeTouchState::Update(bool down, std::int32_t dsX, std::int32_t dsY) noexcept
    {
        const bool wasDown = Down;
        PreviousDown = wasDown;
        PreviousX = X;
        PreviousY = Y;
        Down = down;
        Continued = down && wasDown;
        if (!down)
        {
            // 0202A058: no contact clears the sums and the history.
            DeltaHistoryX.fill(0);
            DeltaHistoryY.fill(0);
            Delta4X = 0;
            Delta4Y = 0;
            return;
        }
        X = static_cast<std::uint8_t>(std::clamp(dsX, 0, 255));
        Y = static_cast<std::uint8_t>(std::clamp(dsY, 0, 191));
        if (!Continued)
        {
            // The first frame of a gesture inherits nothing from the last one.
            DeltaHistoryX.fill(0);
            DeltaHistoryY.fill(0);
            Delta4X = 0;
            Delta4Y = 0;
            return;
        }
        const auto dx = static_cast<std::int16_t>(static_cast<std::int32_t>(X) - PreviousX);
        const auto dy = static_cast<std::int16_t>(static_cast<std::int32_t>(Y) - PreviousY);
        DeltaHistoryX = {DeltaHistoryX[1], DeltaHistoryX[2], DeltaHistoryX[3], dx};
        DeltaHistoryY = {DeltaHistoryY[1], DeltaHistoryY[2], DeltaHistoryY[3], dy};
        Delta4X = Sum4AsS16(DeltaHistoryX);
        Delta4Y = Sum4AsS16(DeltaHistoryY);
    }

    void NativeTouchState::UpdateRelative(bool down, std::int32_t dx, std::int32_t dy) noexcept
    {
        const bool wasDown = Down;
        PreviousDown = wasDown;
        PreviousX = X;
        PreviousY = Y;
        Down = down;
        Continued = down && wasDown;
        if (!down || !Continued)
        {
            DeltaHistoryX.fill(0);
            DeltaHistoryY.fill(0);
            Delta4X = 0;
            Delta4Y = 0;
            if (down)
            {
                X = 128;
                Y = 96;
            }
            return;
        }
        const auto cdx = static_cast<std::int16_t>(std::clamp(dx, -255, 255));
        const auto cdy = static_cast<std::int16_t>(std::clamp(dy, -191, 191));
        X = static_cast<std::uint8_t>(std::clamp(static_cast<std::int32_t>(X) + cdx, 0, 255));
        Y = static_cast<std::uint8_t>(std::clamp(static_cast<std::int32_t>(Y) + cdy, 0, 191));
        DeltaHistoryX = {DeltaHistoryX[1], DeltaHistoryX[2], DeltaHistoryX[3], cdx};
        DeltaHistoryY = {DeltaHistoryY[1], DeltaHistoryY[2], DeltaHistoryY[3], cdy};
        Delta4X = Sum4AsS16(DeltaHistoryX);
        Delta4Y = Sum4AsS16(DeltaHistoryY);
    }

    void MouseStylus::Step(float pixelDx, float pixelDy, NativeTouchState& touch) noexcept
    {
        if (!std::isfinite(pixelDx) || !std::isfinite(pixelDy))
        {
            pixelDx = pixelDy = 0;
        }
        _carryX += pixelDx * DsUnitsPerPixel;
        _carryY += pixelDy * DsUnitsPerPixel;
        const auto dx = static_cast<std::int32_t>(std::trunc(_carryX));
        const auto dy = static_cast<std::int32_t>(std::trunc(_carryY));
        _carryX -= static_cast<float>(dx);
        _carryY -= static_cast<float>(dy);
        if (pixelDx != 0 || pixelDy != 0)
        {
            _idle = 0;
        }
        else if (_idle < IdleStepsBeforeLift)
        {
            ++_idle;
        }
        const bool down = _idle < IdleStepsBeforeLift;
        if (!down)
        {
            _carryX = _carryY = 0;
        }
        touch.UpdateRelative(down, dx, dy);
    }

    namespace
    {
        std::int32_t ToDs(float normalized, float origin, float extent, std::int32_t last) noexcept
        {
            if (!(extent > 0.0F))
            {
                return 0;
            }
            const float local = (normalized - origin) / extent;
            if (!std::isfinite(local))
            {
                return 0;
            }
            return std::clamp(static_cast<std::int32_t>(std::lround(local * static_cast<float>(last))), 0, last);
        }
    }

    std::int32_t ToDsX(float normalizedX, float zoneLeft, float zoneWidth) noexcept
    {
        return ToDs(normalizedX, zoneLeft, zoneWidth, 255);
    }

    std::int32_t ToDsY(float normalizedY, float zoneTop, float zoneHeight) noexcept
    {
        return ToDs(normalizedY, zoneTop, zoneHeight, 191);
    }

    namespace MorphTouchRom
    {
        BoostBranch Arbitrate(bool boosting, bool canTouchBoost, const NativeTouchState& touch) noexcept
        {
            if (boosting || !canTouchBoost || !touch.Continued)
            {
                return BoostBranch::Shoulder;
            }
            const std::int32_t dx = touch.Delta4X;
            const std::int32_t dy = touch.Delta4Y;
            return dx * dx + dy * dy > TouchBoostThresholdSquared ? BoostBranch::TouchBoost : BoostBranch::SkipShoulder;
        }

        RollDelta TouchRoll(std::int16_t delta4X, std::int16_t delta4Y, float scale,
            float rollFbX, float rollFbZ, float rollLrX, float rollLrZ) noexcept
        {
            const float dx = static_cast<float>(delta4X);
            const float dy = static_cast<float>(delta4Y);
            RollDelta result{};
            result.X -= dy * scale * rollFbX;
            result.Z -= dy * scale * rollFbZ;
            result.X -= dx * scale * rollLrX;
            result.Z -= dx * scale * rollLrZ;
            return result;
        }

        RollDelta TouchBoostImpulse(std::int32_t dx, std::int32_t dy, float boostSpeedMax,
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
}
