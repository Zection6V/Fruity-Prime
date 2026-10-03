#include "LauncherNoise.hpp"
#include "../../NativeRuntime/Rhi/OpenGL/OpenGlLauncherNoise.hpp"

namespace MphRead::Mods::Render
{
    using Gpu = ::MphRead::NativeRuntime::Rhi::OpenGL::OpenGlLauncherNoise;
    std::int32_t LauncherNoise::Texture() noexcept { return Gpu::Texture(); }
    bool LauncherNoise::Step(std::int32_t width, std::int32_t height) { return Gpu::Step(width, height); }
    void LauncherNoise::Release() { Gpu::Release(); }
}
