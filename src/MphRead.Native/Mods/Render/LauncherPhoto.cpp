#include "LauncherPhoto.hpp"
#include "../../NativeRuntime/Rhi/OpenGL/OpenGlLauncherPhoto.hpp"

namespace MphRead::Mods::Render
{
    using Gpu = ::MphRead::NativeRuntime::Rhi::OpenGL::OpenGlLauncherPhoto;
    void LauncherPhoto::Enabled(bool value) noexcept { Gpu::Enabled(value); }
    bool LauncherPhoto::Enabled() noexcept { return Gpu::Enabled(); }
    void LauncherPhoto::Draw(std::int32_t width, std::int32_t height) { Gpu::Draw(width, height); }
}
