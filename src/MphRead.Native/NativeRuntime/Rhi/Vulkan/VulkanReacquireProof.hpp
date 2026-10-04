#pragma once

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // An acquisition alone is not a host completion proof. A successful queue
    // submission must consume its imageAvailable semaphore, and the caller
    // must observe that submission's fence before completing this record.
    // Capture the retirement ceiling at acquisition: a later retired chain
    // is outside this proof even if its frame finishes out of order.
    class VulkanReacquireProof final
    {
    public:
        void Acquired(bool previouslyPresented, std::uint64_t retiredThrough)
        {
            if (_submitted) throw std::logic_error("Reacquisition proof is still pending on the GPU.");
            _retiredThrough = previouslyPresented ? retiredThrough : 0;
        }
        void Submitted()
        {
            if (_submitted) throw std::logic_error("Reacquisition proof was submitted twice.");
            _submitted = true;
        }
        [[nodiscard]] bool Pending() const noexcept { return _submitted; }
        [[nodiscard]] std::uint64_t CompleteAfterFenceSignal()
        {
            if (!_submitted) throw std::logic_error("An unsubmitted acquisition is not a completion proof.");
            _submitted = false;
            return std::exchange(_retiredThrough, 0);
        }
    private:
        std::uint64_t _retiredThrough = 0;
        bool _submitted = false;
    };
}
