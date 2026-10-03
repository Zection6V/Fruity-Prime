#pragma once

#include "../../Renderer.hpp"

namespace MphRead::Mods::Render
{
    class DesktopGlContext final
    {
    public:
        DesktopGlContext() = delete;

        static void PreserveWorkingDirectory();
        [[nodiscard]] static ::MphRead::RendererPlatform::WindowSettings Settings(
            bool background = false,
            ::MphRead::RendererPlatform::GraphicsWindowMode graphicsMode
                = ::MphRead::RendererPlatform::GraphicsWindowMode::OpenGL);
    };
}
