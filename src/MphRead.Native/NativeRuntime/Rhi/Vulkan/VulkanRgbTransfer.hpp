#pragma once
#if defined(FRUITY_HAS_VULKAN)
#include "../CommandList.hpp"
#include <vulkan/vulkan.h>
#include <functional>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // Address-only GPU transfer plan. Logical RGB has three-byte texels;
    // native images use RGBA8. Buffer-copy regions gather/scatter RGB bytes.
    struct VulkanRgbTransfer final
    {
        BufferTextureCopy Region;
        VkDeviceSize RowPitch{}, SlicePitch{}, ScratchBytes{};
        static VulkanRgbTransfer Describe(const TextureDesc&, VkDeviceSize bufferBytes, const BufferTextureCopy&);
        [[nodiscard]] VkBufferImageCopy ImageCopy(VkDeviceSize scratchOffset) const;
        void BufferCopies(VkDeviceSize scratchOffset, bool upload,
            const std::function<void(std::span<const VkBufferCopy>)>& emit) const;
    };
}
#endif
