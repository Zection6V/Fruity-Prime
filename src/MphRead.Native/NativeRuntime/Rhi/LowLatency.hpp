#pragma once
#include <cstdint>
#include <string>
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
        std::string_view unavailableReason;
    };
    struct LowLatencyState final
    {
        LowLatencyMode requested = LowLatencyMode::Off, effective = LowLatencyMode::Off;
        LowLatencyProvider provider = LowLatencyProvider::None;
        bool boostSupported = false;
        PacingAuthority authority = PacingAuthority::Generic;
        std::string fallbackReason;
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
            state.fallbackReason = caps.unavailableReason.empty() ? "Boost is unavailable; On is active."
                : std::string(caps.unavailableReason) + " Using generic Low Latency On.";
        }
        if (caps.provider == LowLatencyProvider::Nvidia || caps.provider == LowLatencyProvider::Amd)
            state.authority = PacingAuthority::Native;
        return state;
    }

    enum class LowLatencyMarker : std::uint8_t { InputSample, SimulationStart, SimulationEnd, RenderSubmitStart, RenderSubmitEnd, PresentStart, PresentEnd };
    // Percentiles over one report window, in microseconds.
    struct LowLatencyTimingSummary final
    { std::uint32_t samples = 0; double p50 = 0, p95 = 0, p99 = 0, max = 0; };
    // Where native frame admission spends its time (FRUITY_RENDER_METRICS
    // only): the vkLatencySleepNV call itself, the semaphore polls after it,
    // and the whole admission from the sleep request to a ready frame.
    struct LowLatencyPacingSummary final
    {
        LowLatencyTimingSummary sleepCall, wait, admission;
        std::uint64_t waitTimeouts = 0;
        std::uint64_t windowFrames = 0;
    };
    struct LowLatencyDiagnostics final
    { std::uint64_t sleepCalls = 0, waitCalls = 0, modeCalls = 0, markerCalls = 0, timingReports = 0, frameId = 0, swapchainGeneration = 0;
      std::uint64_t completedMeasurementFrames = 0, abandonedMeasurementFrames = 0, timingQueries = 0;
      std::uint64_t waitTimeouts = 0;
      std::uint32_t revision = 0, minimumIntervalUs = 0;
      LowLatencyPacingSummary pacing; };

    // VkLatencySleepModeInfoNV::minimumIntervalUs for an FPS cap: zero for
    // uncapped (-1) and for the display rate (0), which Reflex then leaves to
    // the present mode; otherwise the cap's period rounded up so the cap is
    // never exceeded.
    // With vsyncHz > 0 (VSync on, Reflex pacing) the refresh period is the
    // floor, so a cap above the refresh rate -- or none -- becomes the
    // refresh rate, and a lower cap is kept.
    [[nodiscard]] constexpr std::uint32_t ReflexMinimumIntervalUs(std::int32_t cap, double vsyncHz = 0.0) noexcept
    {
        const std::uint32_t capped = cap > 0 ? static_cast<std::uint32_t>((1'000'000ULL + static_cast<std::uint64_t>(cap) - 1)
            / static_cast<std::uint64_t>(cap)) : 0;
        if (!(vsyncHz > 0.0)) return capped;
        const double period = 1'000'000.0 / vsyncHz;
        auto refresh = static_cast<std::uint32_t>(period);
        if (static_cast<double>(refresh) < period) ++refresh;
        return capped > refresh ? capped : refresh;
    }
}
