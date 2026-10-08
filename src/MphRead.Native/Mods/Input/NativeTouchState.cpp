#include "NativeTouchState.hpp"

#include "DsTouchSurface.hpp"

#include <algorithm>

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

    void NativeTouchState::Assign(const Reported& reported) noexcept
    {
        Clear();
        Down = reported.Down;
        Continued = reported.Down && reported.Continued;
        PreviousDown = Continued;
        Delta4X = Continued ? reported.Delta4X : static_cast<std::int16_t>(0);
        Delta4Y = Continued ? reported.Delta4Y : static_cast<std::int16_t>(0);
    }

    void NativeTouchState::ClearHistory() noexcept
    {
        DeltaHistoryX.fill(0);
        DeltaHistoryY.fill(0);
        Delta4X = 0;
        Delta4Y = 0;
    }

    bool NativeTouchState::BeginTick(bool down) noexcept
    {
        const bool wasDown = Down;
        PreviousDown = wasDown;
        PreviousX = X;
        PreviousY = Y;
        Down = down;
        Continued = down && wasDown;
        ContactDuration = !down ? 0 : !wasDown ? 1
            : ContactDuration == 0xFFFF ? ContactDuration : static_cast<std::uint16_t>(ContactDuration + 1);
        if (!Continued)
        {
            // 0202A058: no contact, or the first frame of one, clears the
            // sums and the history -- a new gesture inherits nothing.
            ClearHistory();
        }
        return Continued;
    }

    void NativeTouchState::Push(std::int16_t dx, std::int16_t dy) noexcept
    {
        DeltaHistoryX = {DeltaHistoryX[1], DeltaHistoryX[2], DeltaHistoryX[3], dx};
        DeltaHistoryY = {DeltaHistoryY[1], DeltaHistoryY[2], DeltaHistoryY[3], dy};
        Delta4X = Sum4AsS16(DeltaHistoryX);
        Delta4Y = Sum4AsS16(DeltaHistoryY);
    }

    void NativeTouchState::Update(bool down, std::int32_t dsX, std::int32_t dsY) noexcept
    {
        const bool continued = BeginTick(down);
        if (!down)
        {
            return;
        }
        X = static_cast<std::uint8_t>(std::clamp(dsX, 0, DsTouchSurface::MaxX));
        Y = static_cast<std::uint8_t>(std::clamp(dsY, 0, DsTouchSurface::MaxY));
        if (continued)
        {
            Push(static_cast<std::int16_t>(static_cast<std::int32_t>(X) - PreviousX),
                static_cast<std::int16_t>(static_cast<std::int32_t>(Y) - PreviousY));
        }
    }

    void NativeTouchState::UpdateRelative(bool down, std::int32_t dx, std::int32_t dy) noexcept
    {
        if (!BeginTick(down))
        {
            if (down)
            {
                X = DsTouchSurface::CenterX;
                Y = DsTouchSurface::CenterY;
            }
            return;
        }
        const auto cdx = static_cast<std::int16_t>(std::clamp(dx, -DsTouchSurface::MaxX, DsTouchSurface::MaxX));
        const auto cdy = static_cast<std::int16_t>(std::clamp(dy, -DsTouchSurface::MaxY, DsTouchSurface::MaxY));
        X = static_cast<std::uint8_t>(std::clamp(static_cast<std::int32_t>(X) + cdx, 0, DsTouchSurface::MaxX));
        Y = static_cast<std::uint8_t>(std::clamp(static_cast<std::int32_t>(Y) + cdy, 0, DsTouchSurface::MaxY));
        Push(cdx, cdy);
    }
}
