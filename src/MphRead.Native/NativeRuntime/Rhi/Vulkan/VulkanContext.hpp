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
    class VulkanGraphicsDevice;
    class VulkanCommandList;
    class VulkanDeviceState;
    class VulkanSampler;
    class VulkanTexture;
    class VulkanTextureView;

    // Native API objects stay behind the backend's implementation boundary.
    class Context final
    {
    public:
        explicit Context(bool validation);
        Context(bool validation, ::MphRead::RendererPlatform::Window& window, bool allowMaintenance = true);
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
        friend class VulkanGraphicsDevice;
        friend class VulkanCommandList;
        friend class VulkanDeviceState;
        friend class VulkanSampler;
        friend class VulkanTexture;
        friend class VulkanTextureView;
        friend class VulkanBindingLayout;
        friend class VulkanBindingSet;
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };

    // Explicit diagnostic only; does not select a backend for ordinary games.
    int RunFoundationCheck();
    int RunResourceCheck();
}
