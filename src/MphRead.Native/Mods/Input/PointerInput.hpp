#pragma once

#include <cstdint>

namespace MphRead::Mods::Input
{
    class PointerInput final
    {
    public:
        PointerInput() = delete;
        PointerInput(const PointerInput&) = delete;
        PointerInput& operator=(const PointerInput&) = delete;

        [[nodiscard]] static float JumpPixels() noexcept;
        static void JumpPixels(float value) noexcept;

        [[nodiscard]] static bool GuardJumps() noexcept;
        static void GuardJumps(bool value) noexcept;

        [[nodiscard]] static std::int32_t JumpsIgnored() noexcept;
        [[nodiscard]] static bool JumpingPointerSeen() noexcept;

        [[nodiscard]] static float Filter(float delta);
        static void Reset() noexcept;

    private:
        static float _jumpPixels;
        static bool _guardJumps;
        static std::int32_t _jumpsIgnored;
        static bool _jumpingPointerSeen;
    };
}
