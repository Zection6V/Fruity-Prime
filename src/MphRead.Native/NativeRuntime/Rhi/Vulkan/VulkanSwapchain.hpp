#pragma once

#include "../Swapchain.hpp"

#include <memory>

namespace MphRead::RendererPlatform
{
    class Window;
}

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // The implementation owns its Vulkan context, surface, and swapchain.
    // The GLFW window must outlive the returned swapchain.
    [[nodiscard]] std::unique_ptr<Swapchain> CreateSwapchain(
        ::MphRead::RendererPlatform::Window& window, const SwapchainDesc& desc);

    // Visible desktop diagnostic for Phase 13. It does not load game data.
    int RunPresentationCheck(bool forceFallback = false);
}
