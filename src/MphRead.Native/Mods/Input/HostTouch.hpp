#pragma once

#include <cstdint>

namespace MphRead::Mods::Input
{
    // A DS contact another head measured itself (Android's aim finger),
    // already in DS units. While one is published it is the touch source
    // TouchInputAdapter reads, ahead of the desktop's pointer and mouse.
    class HostTouch final
    {
    public:
        HostTouch() = delete;

        static void Publish(bool down, std::int32_t dsX, std::int32_t dsY) noexcept
        {
            _published = true;
            _down = down;
            _x = dsX;
            _y = dsY;
        }
        static void Withdraw() noexcept
        {
            _published = false;
            _down = false;
        }
        [[nodiscard]] static bool Published() noexcept { return _published; }
        [[nodiscard]] static bool Down() noexcept { return _down; }
        [[nodiscard]] static std::int32_t X() noexcept { return _x; }
        [[nodiscard]] static std::int32_t Y() noexcept { return _y; }

    private:
        inline static bool _published = false;
        inline static bool _down = false;
        inline static std::int32_t _x = 0;
        inline static std::int32_t _y = 0;
    };
}
