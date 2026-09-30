#include "DesktopGlContext.hpp"

#include "../Branding.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#if !defined(__ANDROID__)
#include <GLFW/glfw3.h>
#endif

#include <cstdint>
#include <string>

namespace MphRead::Mods::Render
{
    void DesktopGlContext::PreserveWorkingDirectory()
    {
#if !defined(__ANDROID__)
        if (::MphRead::NativeRuntime::IsMacOS())
        {
            ::glfwInitHint(GLFW_COCOA_CHDIR_RESOURCES, GLFW_FALSE);
        }
#endif
    }

    ::MphRead::RendererPlatform::WindowSettings DesktopGlContext::Settings(
        bool background, ::MphRead::RendererPlatform::GraphicsWindowMode graphicsMode)
    {
#if !defined(__ANDROID__)
        ::MphRead::RendererPlatform::InstallGlfwErrorCallback(
            [](std::int32_t code, std::string description)
            {
                ::MphRead::NativeRuntime::ConsoleErrorWriteLine(
                    "[window] GLFW " + std::to_string(code) + ": " + description);
            });
#endif
        PreserveWorkingDirectory();
        const bool mac = ::MphRead::NativeRuntime::IsMacOS();
#if !defined(__ANDROID__)
        if (background && mac)
        {
            ::glfwInitHint(GLFW_COCOA_MENUBAR, GLFW_FALSE);
        }
#else
        (void)background;
#endif

        ::MphRead::RendererPlatform::WindowSettings settings{};
        settings.ClientSize = ::OpenTK::Mathematics::Vector2i(1280, 768);
        settings.Title = std::string(::MphRead::Mods::Branding::Name);
        settings.Profile = mac
            ? ::MphRead::RendererPlatform::WindowSettings::ContextProfile::Any
            : ::MphRead::RendererPlatform::WindowSettings::ContextProfile::Compatability;
        settings.Flags = ::MphRead::RendererPlatform::WindowSettings::ContextFlags::Default;
        settings.ApiMajor = mac ? 2 : 3;
        settings.ApiMinor = mac ? 1 : 2;
        settings.StartVisible = false;
        settings.GraphicsMode = graphicsMode;
        return settings;
    }
}
