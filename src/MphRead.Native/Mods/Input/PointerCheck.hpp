#pragma once

#include <cstdint>
#include <string_view>

namespace MphRead::Mods::Input
{
    class PointerCheck final
    {
    public:
        PointerCheck() = delete;

        [[nodiscard]] static std::int32_t Run();

    private:
        static void Require(bool condition, std::string_view label);
        static void CheckBindings();
        static void CheckMovement();
        static void Frame(float x, float y, bool down, bool independentDown = false, bool acceptsInput = true,
            std::uint32_t id = 1);
        static void CheckZone();
        static void CheckPlayerInput();
        static void CheckSettings();

        inline static std::int32_t _checks = 0;
    };
}
