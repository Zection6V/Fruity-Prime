#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace NCSFCommon::ReplayGain
{
    class GainData
    {
    public:
        GainData() = default;

        GainData(const GainData&) = delete;
        GainData(GainData&&) = delete;
        GainData& operator=(const GainData&) = delete;
        GainData& operator=(GainData&&) = delete;

        [[nodiscard]] std::span<std::int32_t> Accum() noexcept;

        [[nodiscard]] double PeakSample() const noexcept;
        void PeakSample(double value) noexcept;

    private:
        std::array<std::int32_t, 100 * 120> accum{};
        double peakSample = 0.0;
    };
}
