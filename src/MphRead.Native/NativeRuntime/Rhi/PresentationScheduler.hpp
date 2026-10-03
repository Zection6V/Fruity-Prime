#pragma once
#include "Swapchain.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string_view>

namespace MphRead::NativeRuntime::Rhi
{
    enum class LowLatencyMode : std::uint8_t { Off, On, OnBoost };
    enum class LowLatencyProvider : std::uint8_t { None, Generic, Nvidia, Amd };
    enum class PacingAuthority : std::uint8_t { Generic, Native };
    struct LowLatencyCapabilities final
    {
        bool supportsLowLatency = false;
        bool supportsBoost = false;
        LowLatencyProvider provider = LowLatencyProvider::None;
    };
    struct LowLatencyState final
    {
        LowLatencyMode requested = LowLatencyMode::Off, effective = LowLatencyMode::Off;
        LowLatencyProvider provider = LowLatencyProvider::None;
        bool boostSupported = false;
        PacingAuthority authority = PacingAuthority::Generic;
        std::string_view fallbackReason;
        bool operator==(const LowLatencyState&) const = default;
    };
    struct PresentationWaitStatistics final { std::uint64_t count = 0, nanoseconds = 0; };
    inline LowLatencyState ResolveLowLatency(LowLatencyMode requested, LowLatencyCapabilities caps)
    {
        LowLatencyState state{requested, LowLatencyMode::Off, LowLatencyProvider::None, caps.supportsBoost};
        if (requested == LowLatencyMode::Off) return state;
        if (!caps.supportsLowLatency)
        { state.fallbackReason = "Low latency is unavailable on this renderer."; return state; }
        state.effective = requested;
        state.provider = caps.provider;
        if (requested == LowLatencyMode::OnBoost && !caps.supportsBoost)
        {
            state.effective = LowLatencyMode::On;
            state.fallbackReason = "Boost is unavailable; On is active.";
        }
        if (caps.provider == LowLatencyProvider::Nvidia || caps.provider == LowLatencyProvider::Amd)
            state.authority = PacingAuthority::Native;
        return state;
    }

    // Presentation deadlines are independent of the simulation accumulator
    // and of lifetime fences. One authority owns pacing for a frame. Present
    // IDs/target times name accepted presents, not reusable command slots.
    class PresentationScheduler final
    {
    public:
        using Clock = std::chrono::steady_clock;
        using Time = Clock::time_point;
        struct Configuration final
        {
            int frameCap = -1; // zero: display rate; -1: unlimited
            double refreshHz = 60;
            PresentMode presentMode = PresentMode::Fifo;
            PacingAuthority authority = PacingAuthority::Generic;
            bool operator==(const Configuration&) const = default;
        };
        void Configure(Configuration value)
        {
            if (!std::isfinite(value.refreshHz) || value.refreshHz <= 0) value.refreshHz = 60;
            if (_configuration == value) return;
            _configuration = value; ResetDeadline();
        }
        [[nodiscard]] Time Deadline(Time now) const noexcept
        {
            if (_configuration.authority == PacingAuthority::Native || !_hasDeadline) return now;
            return std::max(now, _deadline);
        }
        void Accepted(Time now) noexcept
        {
            ++_acceptedPresentId;
            const auto period = Period();
            if (period == Clock::duration::zero() || _configuration.authority == PacingAuthority::Native)
            { ResetDeadline(); return; }
            // Keep the phase when one frame is late. A stall, restore or
            // rate/backend change starts a fresh deadline, never a catch-up burst.
            if (!_hasDeadline || now > _deadline + period * 2) _deadline = now;
            _deadline += period;
            _hasDeadline = true;
        }
        void Unavailable() noexcept { ResetDeadline(); }
        [[nodiscard]] std::uint64_t PreviousAcceptedPresentId() const noexcept { return _acceptedPresentId; }
        [[nodiscard]] Time TargetDisplayTime(Time now) const noexcept { return Deadline(now); }
        // Only a bounded wait for the previous accepted ID belongs to pacing;
        // present-fence/image reacquisition lifetime ownership is untouched.
        static constexpr std::chrono::nanoseconds FrameBudgetWait{2'000'000};
    private:
        [[nodiscard]] Clock::duration Period() const noexcept
        {
            double rate = _configuration.frameCap > 0 ? _configuration.frameCap : 0;
            if (_configuration.presentMode == PresentMode::Fifo)
                rate = rate > 0 ? std::min(rate, _configuration.refreshHz) : _configuration.refreshHz;
            else if (_configuration.frameCap == 0) rate = _configuration.refreshHz;
            return rate > 0 ? std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1 / rate)) : Clock::duration::zero();
        }
        void ResetDeadline() noexcept { _hasDeadline = false; _deadline = {}; }
        Configuration _configuration;
        Time _deadline{};
        std::uint64_t _acceptedPresentId = 0;
        bool _hasDeadline = false;
    };
}
