#pragma once

#include <array>
#include "Mods/SimulationRate.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Mods::Render
{
    class FrameTiming final
    {
    public:
        FrameTiming() = delete;
        FrameTiming(const FrameTiming&) = delete;
        FrameTiming& operator=(const FrameTiming&) = delete;

        static constexpr std::int32_t SimulationHz = SimulationRate::Hz;
        static constexpr double StepSeconds = SimulationRate::StepSeconds;
        static constexpr std::int32_t MaxCatchUpSteps = 5;

        [[nodiscard]] static std::int32_t FrameRateCap() noexcept;
        static void SetFrameRateCap(std::int32_t value) noexcept;

        // VSync, apart from the cap: on presents FIFO (no tearing, never
        // above the refresh rate), off presents Immediate. The cap is then
        // only ever a number or Unlimited. DisplayRate survives as the old
        // saved/typed value "display", which means VSync on and no cap.
        [[nodiscard]] static bool VSync() noexcept;
        static void SetVSync(bool value) noexcept;
        // -vsync on the command line: settings.json is read after the flags,
        // and must not overwrite what was typed for this run.
        static void ForceVSync(bool value) noexcept;
        [[nodiscard]] static bool VSyncForced() noexcept;

        static constexpr std::int32_t DisplayRate = 0;
        static constexpr std::int32_t Unlimited = -1;
        static constexpr std::int32_t MinCap = 30;
        // The rates the settings offer, in Hz: the FPS limit's stops, and the
        // Animation row's, which reads Unlimited as the display's own rate.
        static constexpr std::array<std::int32_t, 16> RateStops{
            30, 60, 75, 90, 100, 120, 144, 165, 180, 200, 240,
            360, 480, 540, 1000, Unlimited};

        [[nodiscard]] static bool Active() noexcept;
        [[nodiscard]] static std::int32_t StepsThisFrame() noexcept;
        // How far into the next simulation step this frame's picture is, [0, 1).
        [[nodiscard]] static double StepPhase() noexcept;
        // This frame's elapsed time in simulation steps, for what moves per
        // drawn frame at a per-step speed (the free camera). Capped as the
        // steps are; a stall counts as one.
        [[nodiscard]] static double FrameSteps() noexcept;

        [[nodiscard]] static std::int64_t TotalSteps() noexcept;
        [[nodiscard]] static std::int64_t TotalFrames() noexcept;
        [[nodiscard]] static std::int64_t DroppedSteps() noexcept;
        [[nodiscard]] static std::int64_t Stalls() noexcept;
        [[nodiscard]] static std::array<std::int64_t, MaxCatchUpSteps + 1>& StepHistogram() noexcept;
        [[nodiscard]] static double MeasuredSimulationHz() noexcept;
        [[nodiscard]] static double MeasuredFrameHz() noexcept;

        static void ResetDiagnostics() noexcept;
        [[nodiscard]] static std::string Describe();

        static void Reset() noexcept;
        static std::int32_t Advance(double elapsedSeconds);

        [[nodiscard]] static std::int32_t ParseCap(
            const std::optional<std::string>& value, std::int32_t fallback) noexcept;
        [[nodiscard]] static std::int32_t ParseSavedCap(
            const std::optional<std::string>& value, std::int32_t fallback) noexcept;
        [[nodiscard]] static std::string CapString(std::int32_t cap);

    private:
        static constexpr double StallSeconds = 0.25;

        static void Tally(double elapsedSeconds, std::int32_t steps);
        static void ReportWindow();

        static std::int32_t _frameRateCap;
        static bool _vsync;
        static bool _vsyncForced;
        static bool _active;
        static double _accumulator;
        static std::int32_t _stepsThisFrame;
        static double _frameSteps;

        static std::int64_t _totalSteps;
        static std::int64_t _totalFrames;
        static std::int64_t _droppedSteps;
        static std::int64_t _stalls;
        static std::array<std::int64_t, MaxCatchUpSteps + 1> _stepHistogram;
        static double _measuredSimulationHz;
        static double _measuredFrameHz;

        static double _windowSeconds;
        static std::int64_t _windowSteps;
        static std::int64_t _windowFrames;
        static std::int32_t _windowsSinceReport;
        static std::int64_t _reportedDrops;
        static std::int64_t _reportedStalls;
    };
}
