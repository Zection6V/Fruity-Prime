#pragma once
#if defined(FRUITY_HAS_VULKAN)
#include "VulkanDescriptorAllocator.hpp"
#include "VulkanUploadArena.hpp"
#include "../FrameContext.hpp"
#include <array>
#include <functional>
#include <memory>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // Native command/fence/arena ownership for one recording stream. Logical
    // RHI Begin/End and draw-state restoration remain with the command list.
    class VulkanCommandSlots final
    {
    public:
        struct Dispatch final
        {
            VkDevice Device = VK_NULL_HANDLE;
            std::uint32_t QueueFamily = 0;
            PFN_vkCreateCommandPool CreatePool = nullptr;
            PFN_vkDestroyCommandPool DestroyPool = nullptr;
            PFN_vkAllocateCommandBuffers Allocate = nullptr;
            PFN_vkCreateFence CreateFence = nullptr;
            PFN_vkDestroyFence DestroyFence = nullptr;
            PFN_vkGetFenceStatus FenceStatus = nullptr;
            PFN_vkWaitForFences Wait = nullptr;
            PFN_vkResetFences ResetFence = nullptr;
            PFN_vkResetCommandPool ResetPool = nullptr;
            PFN_vkBeginCommandBuffer Begin = nullptr;
            PFN_vkEndCommandBuffer End = nullptr;
            std::function<SubmissionSerial(const VkSubmitInfo2&, VkFence)> Submit;
            std::function<SubmissionSerial()> Completed;
            std::function<void()> HostWait, Collect;
            std::function<void(VkCommandBuffer)> Name;
            std::function<std::unique_ptr<VulkanUploadArena>()> MakeUploads;
            std::function<std::unique_ptr<VulkanDescriptorAllocator>()> MakeDescriptors;
        };
        explicit VulkanCommandSlots(Dispatch dispatch);
        ~VulkanCommandSlots() { Close(); }
        VulkanCommandSlots(const VulkanCommandSlots&) = delete;
        VulkanCommandSlots& operator=(const VulkanCommandSlots&) = delete;
        void Begin();
        void End();
        void Submit();
        void WaitAll();
        [[nodiscard]] bool PollComplete() const;
        [[nodiscard]] bool CanBeginWithoutWait() const;
        // deviceDrained is the session's completed idle/device-loss boundary.
        // Ordinary close drains only this stream's submitted slots.
        void Close(bool deviceDrained = false) noexcept;
        [[nodiscard]] VkCommandBuffer Buffer() const noexcept { return _slots[_current].Buffer; }
        [[nodiscard]] VulkanUploadArena& Uploads() const;
        [[nodiscard]] VulkanDescriptorAllocator& Descriptors() const;
    private:
        enum class State { Idle, Recording, Executable, Pending };
        struct Slot final
        {
            VkCommandPool Pool = VK_NULL_HANDLE;
            VkCommandBuffer Buffer = VK_NULL_HANDLE;
            VkFence Fence = VK_NULL_HANDLE;
            State Status = State::Idle;
            SubmissionSerial LastUse{};
            std::unique_ptr<VulkanUploadArena> Uploads;
            std::unique_ptr<VulkanDescriptorAllocator> Descriptors;
        };
        void RequireOpen() const;
        void Complete(Slot& slot);
        [[nodiscard]] bool FenceReady(const Slot& slot) const;
        void DestroyNative() noexcept;
        Dispatch _dispatch;
        std::array<Slot, FramesInFlight> _slots;
        std::size_t _current = 0;
        bool _closed = false;
    };
}
#endif
