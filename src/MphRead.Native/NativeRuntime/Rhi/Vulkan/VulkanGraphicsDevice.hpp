#pragma once

#include "../GraphicsDevice.hpp"
#include "../Swapchain.hpp"

#include <memory>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    class Context;

    // Vulkan resources are owned by a graphics device that must not outlive
    // their Context.
    [[nodiscard]] std::unique_ptr<GraphicsDevice> CreateGraphicsDevice(Context& context);
    // Backend diagnostic; keeps native descriptor handles out of the RHI.
    void CheckBindingAllocations(GraphicsDevice& device);
    void CheckShaderModules(GraphicsDevice& device);
    void CheckGraphicsPipelines(GraphicsDevice& device);
    // Submit the device's recorded work and show its window target: blitted
    // upright into the swapchain's next image (black before anything drew).
    void PresentWindow(GraphicsDevice& device, Swapchain& swapchain);
}
