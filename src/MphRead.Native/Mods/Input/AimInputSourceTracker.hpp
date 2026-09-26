#pragma once

#include <cstdint>

namespace MphRead::Mods::Input
{
    enum class AimInputSource : std::int32_t { None, Mouse, Touch, Gamepad };

    // AimInputSource.ToString().
    [[nodiscard]] const char* ToString(AimInputSource value) noexcept;

    class AimInputSourceTracker final
    {
    public:
        AimInputSourceTracker() = delete;

        [[nodiscard]] static AimInputSource Current() noexcept { return _current; }
        [[nodiscard]] static std::int64_t Revision() noexcept { return _revision; }
        static void Pointer(float x, float y, bool touch, std::int64_t milliseconds) noexcept;
        static void Stick(float x, float y, std::int64_t milliseconds) noexcept;
        static void Reset() noexcept;

    private:
        inline static AimInputSource _current = AimInputSource::None;
        inline static std::int64_t _revision = 0;
        inline static std::int64_t _claimStart = -1;
    };
}
