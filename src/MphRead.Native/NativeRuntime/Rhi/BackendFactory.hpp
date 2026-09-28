#pragma once

#include "Backend.hpp"
#include "Swapchain.hpp"

#include <memory>

namespace MphRead::RendererPlatform
{
    class Window;
}

namespace MphRead::NativeRuntime::Rhi
{
    class BackendFactory final
    {
    public:
        BackendFactory() = delete;

        [[nodiscard]] static std::unique_ptr<Swapchain> CreateSwapchain(
            GraphicsBackend backend, ::MphRead::RendererPlatform::Window& window,
            const SwapchainDesc& desc);
    };
}
