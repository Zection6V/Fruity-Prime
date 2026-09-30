#pragma once

#include <cstdint>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    class OpenGlLauncherNoise final
    {
    public:
        OpenGlLauncherNoise() = delete;

        [[nodiscard]] static std::int32_t Texture() noexcept;
        [[nodiscard]] static bool Step(std::int32_t windowWidth, std::int32_t windowHeight);
        static void Release();

    private:
        [[nodiscard]] static bool Upload();

        static constexpr std::int32_t Name = 1'000'002;
    };
}
