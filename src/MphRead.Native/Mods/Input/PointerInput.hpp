#pragma once

#include <cstdint>
#include <utility>

namespace MphRead::Mods::Input
{
    class PointerInput final
    {
    public:
        PointerInput() = delete;

        [[nodiscard]] static bool StylusMode() noexcept { return _stylusMode; }
        static void StylusMode(bool value) noexcept { _stylusMode = value; }
        [[nodiscard]] static bool GuardJumps() noexcept { return _guardJumps; }
        static void GuardJumps(bool value) noexcept { _guardJumps = value; }
        [[nodiscard]] static float JumpPixels() noexcept { return _jumpPixels; }
        static void JumpPixels(float value) noexcept { _jumpPixels = value; }
        [[nodiscard]] static std::int32_t JumpsIgnored() noexcept { return _jumpsIgnored; }
        [[nodiscard]] static bool JumpingPointerSeen() noexcept { return _jumpingPointerSeen; }

        [[nodiscard]] static std::pair<float, float> Filter(float x, float y);
        static void Reset() noexcept;

    private:
        inline static bool _stylusMode = false;
        inline static bool _guardJumps = true;
        inline static float _jumpPixels = 600;
        inline static std::int32_t _jumpsIgnored = 0;
        inline static bool _jumpingPointerSeen = false;
    };
}
