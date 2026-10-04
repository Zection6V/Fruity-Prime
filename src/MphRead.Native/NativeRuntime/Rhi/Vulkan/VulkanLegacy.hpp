#pragma once

#if defined(FRUITY_HAS_VULKAN)
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <cstdint>
#include <span>

// A device below Vulkan 1.3 -- a Mali-G78 stops at 1.1, with neither dynamic
// rendering nor synchronization2 as extensions -- runs the backend unchanged:
// the 1.3 entry points it calls are these, installed in the dispatch table in
// their place. Rendering is a render pass and framebuffer made from the
// VkRenderingInfo (both cached), barriers and submits are the 1.0 calls, and
// vertex strides are baked into the scene pipelines by the command list.
//
// One legacy device at a time: the process has one.
namespace MphRead::NativeRuntime::Rhi::Vulkan::Legacy
{
    // Loads the 1.0/1.1 entry points the shims stand on. Before any image
    // view is made: a view's format is how a rendering's attachment is known.
    void Install(VkDevice device, PFN_vkGetDeviceProcAddr proc);
    // Destroys the cached render passes and framebuffers; before vkDestroyDevice.
    void Uninstall(VkDevice device) noexcept;

    // A render pass compatible with a rendering into these formats, for
    // pipeline creation (compatibility is formats and sample counts only).
    [[nodiscard]] VkRenderPass CompatibleRenderPass(std::span<const VkFormat> colors, VkFormat depth,
        bool stencil, VkSampleCountFlagBits samples);

    // synchronization2's flags and layouts, in 1.0's terms. An empty stage
    // mask is the top of the pipe as a source, the bottom as a destination.
    [[nodiscard]] VkPipelineStageFlags ToStages(VkPipelineStageFlags2 stages, bool source) noexcept;
    [[nodiscard]] VkAccessFlags ToAccess(VkAccessFlags2 access) noexcept;
    [[nodiscard]] VkImageLayout ToLayout(VkImageLayout layout, VkImageAspectFlags aspect) noexcept;

    VKAPI_ATTR void VKAPI_CALL CmdPipelineBarrier2(VkCommandBuffer, const VkDependencyInfo*);
    VKAPI_ATTR VkResult VKAPI_CALL QueueSubmit2(VkQueue, std::uint32_t, const VkSubmitInfo2*, VkFence);
    VKAPI_ATTR void VKAPI_CALL CmdWriteTimestamp2(VkCommandBuffer, VkPipelineStageFlags2, VkQueryPool, std::uint32_t);
    VKAPI_ATTR void VKAPI_CALL CmdBeginRendering(VkCommandBuffer, const VkRenderingInfo*);
    VKAPI_ATTR void VKAPI_CALL CmdEndRendering(VkCommandBuffer);
    // The strides are dropped: the bound pipeline has them.
    VKAPI_ATTR void VKAPI_CALL CmdBindVertexBuffers2(VkCommandBuffer, std::uint32_t, std::uint32_t,
        const VkBuffer*, const VkDeviceSize*, const VkDeviceSize*, const VkDeviceSize*);
    VKAPI_ATTR void VKAPI_CALL GetDeviceBufferMemoryRequirements(VkDevice,
        const VkDeviceBufferMemoryRequirements*, VkMemoryRequirements2*);
    VKAPI_ATTR void VKAPI_CALL GetDeviceImageMemoryRequirements(VkDevice,
        const VkDeviceImageMemoryRequirements*, VkMemoryRequirements2*);
    // Record each view's format, and drop the framebuffers that held a view.
    VKAPI_ATTR VkResult VKAPI_CALL CreateImageView(VkDevice, const VkImageViewCreateInfo*,
        const VkAllocationCallbacks*, VkImageView*);
    VKAPI_ATTR void VKAPI_CALL DestroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*);
}
#endif

