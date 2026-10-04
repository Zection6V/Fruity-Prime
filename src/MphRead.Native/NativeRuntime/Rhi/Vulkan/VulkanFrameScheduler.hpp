#pragma once

#include "../Submission.hpp"
#include "../PresentationScheduler.hpp"
#include <cstdlib>

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <optional>
#include <thread>
#include <limits>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // Tracks actual graphics queue submissions, including a marker following
    // external producers on that queue. It does not own frame/descriptor slots.
    class VulkanFrameScheduler final
    {
    public:
        struct Dispatch final
        {
            VkDevice Device;
            VkQueue Queue;
            PFN_vkCreateSemaphore CreateSemaphore;
            PFN_vkDestroySemaphore DestroySemaphore;
            PFN_vkQueueSubmit2 QueueSubmit;
            PFN_vkGetSemaphoreCounterValue CounterValue;
            void (*CheckResult)(VkResult, const char*);
            PFN_vkWaitSemaphores Wait = nullptr;
            std::function<std::optional<std::uint64_t>()> Attribution;
            std::function<void()> RenderSubmitStart;
        };

        explicit VulkanFrameScheduler(Dispatch dispatch) : _dispatch(dispatch)
        {
            if (!_dispatch.Device || !_dispatch.Queue || !_dispatch.CreateSemaphore
                || !_dispatch.DestroySemaphore || !_dispatch.QueueSubmit || !_dispatch.CounterValue
                || !_dispatch.CheckResult)
                throw std::invalid_argument("Incomplete Vulkan queue-scheduler dispatch.");
            // Queue recording/submission is owned by the constructing thread.
            // Reserve the ordinary present signal plus timeline marker once.
            _signals.reserve(4);
            VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
            type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            VkSemaphoreCreateInfo create{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            create.pNext = &type;
            _dispatch.CheckResult(_dispatch.CreateSemaphore(_dispatch.Device, &create, nullptr, &_timeline),
                "vkCreateSemaphore(submission timeline)");
            if (!_timeline) throw std::runtime_error("Vulkan queue scheduler has no timeline semaphore.");
        }

        ~VulkanFrameScheduler()
        {
            // The owner drains the queue before destroying the scheduler.
            _dispatch.DestroySemaphore(_dispatch.Device, _timeline, nullptr);
        }
        VulkanFrameScheduler(const VulkanFrameScheduler&) = delete;
        VulkanFrameScheduler& operator=(const VulkanFrameScheduler&) = delete;

        SubmissionSerial Submit(const VkSubmitInfo2& work, VkFence fence = VK_NULL_HANDLE, bool mark = true)
        {
            if (std::this_thread::get_id() != _submissionThread || _submitting)
                throw std::logic_error("Vulkan queue submission must be non-reentrant on its owning thread.");
            struct SubmissionScope final
            {
                bool& Active;
                explicit SubmissionScope(bool& active) : Active(active) { Active = true; }
                ~SubmissionScope() { Active = false; }
            } scope(_submitting);
            if (work.sType != VK_STRUCTURE_TYPE_SUBMIT_INFO_2
                || (work.waitSemaphoreInfoCount && !work.pWaitSemaphoreInfos)
                || (work.commandBufferInfoCount && !work.pCommandBufferInfos)
                || (work.signalSemaphoreInfoCount && !work.pSignalSemaphoreInfos))
                throw std::invalid_argument("Invalid Vulkan queue submission arrays.");
            if (work.signalSemaphoreInfoCount == std::numeric_limits<std::uint32_t>::max())
                throw std::invalid_argument("Vulkan submission has no room for its timeline signal.");
            const auto serial = _progress.Next();
            const auto required = static_cast<std::size_t>(work.signalSemaphoreInfoCount) + 1;
            if (required > _signals.capacity())
            { _signals.reserve(required); ++_signalStorageGrowths; }
            _signals.clear();
            if (work.signalSemaphoreInfoCount)
                _signals.assign(work.pSignalSemaphoreInfos,
                    work.pSignalSemaphoreInfos + work.signalSemaphoreInfoCount);
            VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
            signal.semaphore = _timeline;
            signal.value = serial.Value;
            signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            _signals.push_back(signal);
            VkSubmitInfo2 submit = work;
            submit.signalSemaphoreInfoCount = static_cast<std::uint32_t>(_signals.size());
            submit.pSignalSemaphoreInfos = _signals.data();
            VkLatencySubmissionPresentIdNV attribution{VK_STRUCTURE_TYPE_LATENCY_SUBMISSION_PRESENT_ID_NV};
            if (_dispatch.Attribution)
                if (const auto id = _dispatch.Attribution())
                { attribution.presentID = *id; attribution.pNext = submit.pNext; submit.pNext = &attribution; }
            if (mark && _dispatch.RenderSubmitStart) _dispatch.RenderSubmitStart();
            _dispatch.CheckResult(_dispatch.QueueSubmit(_dispatch.Queue, 1, &submit, fence),
                "vkQueueSubmit2(submission timeline)");
            _progress.Submitted(serial);
            return serial;
        }

        SubmissionSerial MarkExternalWork(bool mark = true)
        {
            const VkSubmitInfo2 marker{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
            return Submit(marker, VK_NULL_HANDLE, mark);
        }

        SubmissionSerial Poll()
        {
            std::uint64_t complete = 0;
            _dispatch.CheckResult(_dispatch.CounterValue(_dispatch.Device, _timeline, &complete),
                "vkGetSemaphoreCounterValue(submission timeline)");
            _progress.Complete({complete});
            return _progress.Completed();
        }

        [[nodiscard]] SubmissionSerial Submitted() const noexcept { return _progress.Submitted(); }
        [[nodiscard]] bool WaitForLatest(std::uint64_t timeoutNanoseconds)
        {
            const auto serial = Submitted();
            if (serial <= Poll()) return true;
            if (!_dispatch.Wait) return false;
            VkSemaphoreWaitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
            wait.semaphoreCount = 1; wait.pSemaphores = &_timeline; wait.pValues = &serial.Value;
            const auto start = _measureWaits ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            const auto result = _dispatch.Wait(_dispatch.Device, &wait, timeoutNanoseconds);
            if (_measureWaits)
            { ++_waits.count; _waits.nanoseconds += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count(); }
            if (result == VK_TIMEOUT) return false;
            _dispatch.CheckResult(result, "vkWaitSemaphores(presentation frame budget)");
            return Poll() >= serial;
        }
        [[nodiscard]] SubmissionSerial Completed() const noexcept { return _progress.Completed(); }
        [[nodiscard]] PresentationWaitStatistics PresentationWaits() const noexcept { return _waits; }
        [[nodiscard]] std::uint64_t SignalStorageGrowths() const noexcept { return _signalStorageGrowths; }

    private:
        Dispatch _dispatch;
        VkSemaphore _timeline = VK_NULL_HANDLE;
        SubmissionProgress _progress;
        const std::thread::id _submissionThread = std::this_thread::get_id();
        bool _submitting = false;
        std::vector<VkSemaphoreSubmitInfo> _signals;
        std::uint64_t _signalStorageGrowths = 0;
        bool _measureWaits = std::getenv("FRUITY_RENDER_METRICS") != nullptr;
        PresentationWaitStatistics _waits;
    };
}
