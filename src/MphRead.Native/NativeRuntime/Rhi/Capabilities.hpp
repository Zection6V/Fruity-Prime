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
        std::uint32_t maxBindingSets = 0;
        float maxSamplerAnisotropy = 1.0F;
        bool supportsCompute = false;
        bool supportsTimestampQueries = false;
        bool supportsAnisotropy = false;
        bool supportsWireframe = false;
        bool supportsDepthClamp = false;

        bool operator==(const Capabilities&) const = default;
    };
}
