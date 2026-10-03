#include "VulkanTransferScratch.hpp"
#if defined(FRUITY_HAS_VULKAN)
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    VulkanTransferScratch::VulkanTransferScratch(Dispatch dispatch, VkDeviceSize pageSize)
        : _dispatch(std::move(dispatch)), _pageSize(pageSize)
    {
        if (!_dispatch.Create || !_dispatch.Destroy || !pageSize || pageSize % 4)
            throw std::invalid_argument("Invalid GPU transfer scratch dispatch or page size.");
    }
    VulkanTransferScratch::Slice VulkanTransferScratch::Allocate(VkDeviceSize bytes)
    {
        if (_closed || _pending) throw std::logic_error("GPU transfer scratch is closed or still submitted.");
        if (!bytes || bytes > std::numeric_limits<VkDeviceSize>::max() - 3)
            throw std::out_of_range("GPU transfer scratch size overflows.");
        const auto size = (bytes + 3) & ~VkDeviceSize{3};
        for (std::size_t index = _active; index < _pages.size(); ++index)
        {
            auto& entry = _pages[index];
            if (size > entry.Native.Size - entry.Used) continue;
            _active = index; const auto offset = entry.Used; entry.Used += size;
            return {entry.Native.Buffer, offset, size};
        }
        const auto pageSize = std::max(size, _pageSize);
        if (pageSize > std::numeric_limits<VkDeviceSize>::max() - _reserved)
            throw std::out_of_range("GPU transfer scratch reservation overflows.");
        _pages.reserve(_pages.size() + 1); // No throwing adoption after allocation.
        const auto page = _dispatch.Create(pageSize);
        if (!page.Buffer || !page.Allocation || page.Size != pageSize)
        {
            _dispatch.Destroy(page);
            throw std::runtime_error("Incomplete GPU transfer scratch page.");
        }
        _pages.push_back({page, size}); _active = _pages.size() - 1; _reserved += pageSize;
        return {page.Buffer, 0, size};
    }
    void VulkanTransferScratch::Submitted(SubmissionSerial serial)
    {
        if (_closed || _pending || !serial.Value || serial <= _lastUse)
            throw std::logic_error("Invalid GPU transfer scratch submission.");
        _lastUse = serial; _pending = true;
    }
    void VulkanTransferScratch::ResetAfterCompletion(SubmissionSerial completed)
    {
        if (_closed || completed < _lastUse) throw std::logic_error("GPU transfer scratch is not complete.");
        for (auto& entry : _pages) entry.Used = 0;
        _active = 0; _pending = false;
    }
    void VulkanTransferScratch::Close() noexcept
    {
        if (_closed) return;
        for (auto& entry : _pages) _dispatch.Destroy(entry.Native);
        _pages.clear(); _reserved = 0; _closed = true;
    }
}
#endif
