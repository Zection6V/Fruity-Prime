#pragma once

#include <cstdint>

namespace MphRead::Mods::Render
{
    class LauncherPhoto final
    {
    public:
        LauncherPhoto() = delete;

        static void Enabled(bool value) noexcept;
        [[nodiscard]] static bool Enabled() noexcept;
        static void Draw(std::int32_t width, std::int32_t height);
    };
}
