#pragma once

#include <cstdint>

namespace MphRead::NativeRuntime::Rhi
{
    enum class GraphicsBackend : std::uint8_t
    {
        OpenGl,
        Vulkan,
        Metal,
        D3D12
    };
}
