#pragma once

namespace MphRead::NativeRuntime::Rhi
{
    class Texture;
}

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    class WindowUi;
}

namespace MphRead::Mods::Render
{
    // The launcher's window-level drawing when the window presents through
    // Vulkan: one WindowUi on the scene device, shared by the photograph and
    // the overlay, and gone with the window. Null whenever the window is
    // OpenGL's, which keeps its own code paths untouched.
    class VulkanWindowUi final
    {
    public:
        VulkanWindowUi() = delete;

        [[nodiscard]] static bool Active() noexcept;
        [[nodiscard]] static NativeRuntime::Rhi::Vulkan::WindowUi* Get();
        static void Release() noexcept;
    };
}
