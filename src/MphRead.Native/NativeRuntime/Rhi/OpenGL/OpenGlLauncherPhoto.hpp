#pragma once

#include <cstdint>

namespace MphRead::NativeRuntime::Rhi::OpenGL
{
    class OpenGlLauncherPhoto final
    {
    public:
        OpenGlLauncherPhoto() = delete;

        static void Enabled(bool value) noexcept;
        [[nodiscard]] static bool Enabled() noexcept;
        static void Draw(std::int32_t width, std::int32_t height);

    private:
        [[nodiscard]] static bool Ensure();
        [[nodiscard]] static bool EnsureProgram();

        static constexpr std::int32_t Name = 1'000'001;
        static constexpr float Strength = 0.62F;
        static bool _enabled;
        static std::int32_t _texture;
        static std::int32_t _width;
        static std::int32_t _height;
        static bool _tried;
        static std::int32_t _program;
        static bool _programTried;
        static std::int32_t _photoUniform;
        static std::int32_t _strengthUniform;
        static std::int32_t _timeUniform;
        static std::int32_t _viewWidthUniform;
        static std::int32_t _viewHeightUniform;
    };
}
