#pragma once

#include "../Capabilities.hpp"
#include <memory>
#include <string>

namespace MphRead::RendererPlatform
{
    class Window;
}

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    class VulkanSwapchain;

    // Native API objects stay behind the backend's implementation boundary.
    class Context final
    {
    public:
        explicit Context(bool validation);
        Context(bool validation, ::MphRead::RendererPlatform::Window& window);
        ~Context();
        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        [[nodiscard]] const Capabilities& Caps() const noexcept;
        [[nodiscard]] const std::string& DeviceName() const noexcept;
        [[nodiscard]] unsigned ValidationErrors() const noexcept;
        [[nodiscard]] bool ValidationEnabled() const noexcept;
        void WaitIdle();
        void Shutdown();
        void CheckCommandBufferDebugName();
    private:
        friend class VulkanSwapchain;
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };

    // Explicit diagnostic only; does not select a backend for ordinary games.
    int RunFoundationCheck();
}
