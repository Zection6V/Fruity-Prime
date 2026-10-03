#pragma once

#include "Backend.hpp"

#include <cstdint>

namespace MphRead::NativeRuntime::Rhi
{
    struct Capabilities final
    {
        GraphicsBackend backend = GraphicsBackend::OpenGl;
        std::uint32_t maxTexture2DDimension = 0;
        std::uint32_t maxTextureArrayLayers = 0;
        std::uint32_t maxColorAttachments = 0;
        std::uint32_t maxVertexBuffers = 0;
        std::uint32_t maxBindingGroups = 0;
        float maxSamplerAnisotropy = 1.0F;
        bool supportsCompute = false;
        bool supportsTimestampQueries = false;
        bool supportsDebugLabels = false;
        bool supportsAnisotropy = false;
        bool supportsWireframe = false;
        bool supportsDepthClamp = false;
        // Buffer copies of one aspect of a packed depth/stencil image
        // (D24UnormS8Uint, D32FloatS8Uint): depth is 4 bytes a texel (the
        // high 8 bits of a D24 download are undefined), stencil 1 byte.
        // Where false, creating such an image with transfer usage is
        // refused, rather than failing at the copy.
        bool supportsPackedDepthStencilTransfer = false;

        bool operator==(const Capabilities&) const = default;
    };
}
