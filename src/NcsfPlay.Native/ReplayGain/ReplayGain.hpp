#pragma once

#include "FrequencyInfo.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace NCSFCommon::ReplayGain
{
    class ReplayGain
    {
    public:
        static constexpr std::int32_t StepsPerDb = 100;
        static constexpr std::int32_t MaxDb = 120;
        static constexpr std::int32_t MaxSampleValue = 32767;
        static constexpr double Factor24Bit = static_cast<double>(MaxSampleValue) / 0x800000;

        static constexpr std::int32_t YuleOrder = 10;
        static constexpr std::int32_t ButterOrder = 2;
        static constexpr std::int32_t MaxOrder = YuleOrder > ButterOrder ? YuleOrder : ButterOrder;

        static constexpr double PinkRef = 64.82;
        static constexpr double RmsWindowTime = 0.05;
        static constexpr double RmsPercentile = 0.95;
        static constexpr std::int32_t MaxSampFreq = 48000;
        static constexpr std::int32_t MaxSamplesPerWindow =
            static_cast<std::int32_t>(MaxSampFreq * RmsWindowTime + 1);

        static std::shared_ptr<std::vector<std::shared_ptr<FrequencyInfo>>> FreqInfos;

        [[nodiscard]] static double AnalyzeResult(std::span<const std::int32_t> array);
        [[nodiscard]] static bool IsSupportedFormat(std::int32_t sampleRate, std::int32_t sampleSize);

    private:
        ReplayGain() = delete;
    };
}
