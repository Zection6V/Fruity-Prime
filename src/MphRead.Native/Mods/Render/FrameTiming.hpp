#pragma once

#include <array>
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

        static constexpr std::int32_t SimulationHz = 60;
        static constexpr double StepSeconds = 1.0 / SimulationHz;
        static constexpr std::int32_t MaxCatchUpSteps = 5;

        [[nodiscard]] static std::int32_t FrameRateCap() noexcept;
        static void SetFrameRateCap(std::int32_t value) noexcept;

        static constexpr std::int32_t DisplayRate = 0;
        static constexpr std::int32_t MinCap = 30;
        static constexpr std::int32_t MaxCap = 500;

        [[nodiscard]] static bool Active() noexcept;
        [[nodiscard]] static std::int32_t StepsThisFrame() noexcept;

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
        [[nodiscard]] static std::string CapString(std::int32_t cap);

    private:
        static constexpr double StallSeconds = 0.25;

        static void Tally(double elapsedSeconds, std::int32_t steps);
        static void ReportWindow();

        static std::int32_t _frameRateCap;
        static bool _active;
        static double _accumulator;
        static std::int32_t _stepsThisFrame;

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
