#pragma once

#include "../GraphicsDevice.hpp"

#include <memory>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    class Context;

    // Vulkan resources are owned by a graphics device that must not outlive
    // their Context.
    [[nodiscard]] std::unique_ptr<GraphicsDevice> CreateGraphicsDevice(Context& context);
}
