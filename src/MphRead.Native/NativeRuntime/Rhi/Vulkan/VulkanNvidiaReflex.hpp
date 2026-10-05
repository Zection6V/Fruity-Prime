#pragma once
#if defined(FRUITY_HAS_VULKAN)
#include "../LowLatency.hpp"
#include <vulkan/vulkan.h>
#include <array>
#include <chrono>
#include <optional>

namespace MphRead::NativeRuntime::Rhi::Vulkan
{
    // Owned by the swapchain; its timeline belongs to driver sleep, never to
    // queue-completion serials. All calls run on the presentation thread.
    class VulkanNvidiaReflex final
    {
    public:
        struct Dispatch final
        {
            VkDevice device = VK_NULL_HANDLE;
            bool enabled = false;
            std::uint32_t revision = 0;
            std::string reason;
            PFN_vkCreateSemaphore create = nullptr;
            PFN_vkDestroySemaphore destroy = nullptr;
            PFN_vkWaitSemaphores wait = nullptr;
            PFN_vkSetLatencySleepModeNV setMode = nullptr;
            PFN_vkLatencySleepNV sleep = nullptr;
            PFN_vkSetLatencyMarkerNV marker = nullptr;
            PFN_vkGetLatencyTimingsNV timings = nullptr;
        };
        explicit VulkanNvidiaReflex(Dispatch dispatch, std::uint64_t& sequence);
        ~VulkanNvidiaReflex(); // Owner must drain before destroying the timeline.
        void SetSwapchain(VkSwapchainKHR swapchain);
        void SetMode(LowLatencyMode mode, std::uint32_t intervalUs);
        [[nodiscard]] bool BeginFrame(); // bounded poll; does not repeat sleep
        void Mark(LowLatencyMarker marker);
        void FinishFrame();
        void AbandonFrame() noexcept;
        void Shutdown() noexcept;
        [[nodiscard]] bool Available() const noexcept { return _available; }
        [[nodiscard]] bool MeasurementAvailable() const noexcept { return _available && _swapchain; }
        [[nodiscard]] bool PacingActive() const noexcept { return MeasurementAvailable() && _mode != LowLatencyMode::Off; }
        [[nodiscard]] LowLatencyCapabilities Caps() const noexcept
        { return _available && _swapchain ? LowLatencyCapabilities{true, true, LowLatencyProvider::Nvidia}
            : LowLatencyCapabilities{true, false, LowLatencyProvider::Generic, _reason}; }
        [[nodiscard]] std::uint64_t FrameId() const noexcept { return MeasurementAvailable() && _ready ? _frameId : 0; }
        [[nodiscard]] std::optional<std::uint64_t> SubmissionId() const noexcept
        {
            // Every mode uses the extension's implicit frame attribution: the
            // markers carry the frame identity, and the queue's explicit ID is
            // cleared with zero. The spec calls the implicit rules sufficient
            // for the vast majority of applications, and the explicit ID is
            // not harmless here: on NVIDIA 617.14 (RTX 5070 Ti, 540 Hz) a
            // nonzero ID on each submit makes the driver signal the Reflex
            // sleep about one frame late -- admission 2.7 ms against 11 us --
            // which held On and On+Boost at ~280 FPS where Off ran at ~1150.
            // FRUITY_REFLEX_EXPLICIT_SUBMISSION_ID=1 puts the explicit IDs
            // back in On/Boost, as a developer A/B only.
            return _dispatch.enabled && _dispatch.revision >= 3
                ? std::optional(_mode == LowLatencyMode::Off || !_explicitSubmissionId ? std::uint64_t{0} : FrameId())
                : std::nullopt;
        }
        [[nodiscard]] LowLatencyDiagnostics Stats() const noexcept
        {
            auto stats = _stats;
            stats.revision = _dispatch.revision;
            stats.minimumIntervalUs = _mode == LowLatencyMode::Off ? 0 : _interval;
            return stats;
        }
    private:
        void Fail(const char* operation, VkResult result);
        void ApplyMode();
        void PollTimings();
        void RecordPacing(double sleepUs, double waitUs, double admissionUs) noexcept;
        void SummarisePacing() noexcept;
        Dispatch _dispatch;
        std::uint64_t& _sequence;
        VkSemaphore _semaphore = VK_NULL_HANDLE;
        VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
        LowLatencyMode _mode = LowLatencyMode::Off, _applied = LowLatencyMode::Off;
        std::uint32_t _interval = 0, _appliedInterval = 0;
        bool _available = false, _modeApplied = false, _sleepPending = false, _ready = false;
        std::uint64_t _frameId = 0;
        std::uint64_t _lastTimingPoll = 0;
        std::uint32_t _markers = 0;
        std::chrono::steady_clock::time_point _sleepStart{};
        // Developer switches, read once at construction (see the .cpp).
        bool _metrics = false, _explicitSubmissionId = false, _bypassSleep = false;
        // One report window of admission samples, fixed size so the hot path
        // never allocates. Microseconds.
        static constexpr std::size_t PacingWindow = 256;
        std::array<float, PacingWindow> _sleepSamples{}, _waitSamples{}, _admissionSamples{};
        std::size_t _pacingCount = 0;
        std::uint64_t _windowTimeouts = 0;
        double _frameSleepUs = 0, _frameWaitUs = 0;
        std::string _reason;
        LowLatencyDiagnostics _stats;
    };
}
#endif
