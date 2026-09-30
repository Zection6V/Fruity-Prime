#pragma once

#include <cstdint>

namespace MphRead::Mods::Render
{
    class LauncherNoise final
    {
    public:
        LauncherNoise() = delete;

        [[nodiscard]] static std::int32_t Texture() noexcept;
        [[nodiscard]] static bool Step(std::int32_t windowWidth, std::int32_t windowHeight);
        static void Release();
    };
}
