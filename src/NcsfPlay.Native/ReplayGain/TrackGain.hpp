#pragma once

#include "FrequencyInfo.hpp"
#include "GainData.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace NCSFCommon::ReplayGain
{
    class AlbumGain;

    class TrackGain
    {
    public:
        TrackGain(std::int32_t sampleRate, std::int32_t sampleSize);

        TrackGain(const TrackGain&) = delete;
        TrackGain(TrackGain&&) = delete;
        TrackGain& operator=(const TrackGain&) = delete;
        TrackGain& operator=(TrackGain&&) = delete;

        void AnalyzeSamples(
            std::span<const std::int32_t> leftSamples,
            std::span<const std::int32_t> rightSamples);

        [[nodiscard]] double GetGain();
        double GetPeak();

    private:
        friend class AlbumGain;

        [[nodiscard]] static double Sqr(double d) noexcept;

        static void FilterYule(
            std::span<const double> input,
            std::span<double> output,
            std::int32_t& inPos,
            std::int32_t& outPos,
            std::int64_t nSamples,
            std::span<const double> aKernel,
            std::span<const double> bKernel);

        static void FilterButter(
            std::span<const double> input,
            std::span<double> output,
            std::int32_t& inPos,
            std::int32_t& outPos,
            std::int64_t nSamples,
            std::span<const double> aKernel,
            std::span<const double> bKernel);

        void AnalyzeSamples(
            std::span<const double> leftSamples,
            std::span<const double> rightSamples);

        std::int32_t sampleSize = 0;
        NCSFCommon::ReplayGain::GainData GainData;

        std::vector<double> lInPreBuf;
        const std::int32_t lInPrePos;
        std::vector<double> lStepBuf;
        const std::int32_t lStepPos;
        std::vector<double> lOutBuf;
        const std::int32_t lOutPos;

        std::vector<double> rInPreBuf;
        const std::int32_t rInPrePos;
        std::vector<double> rStepBuf;
        const std::int32_t rStepPos;
        std::vector<double> rOutBuf;
        const std::int32_t rOutPos;

        std::int64_t sampleWindow = 0;
        std::int64_t totSamp = 0;
        double lSum = 0.0;
        double rSum = 0.0;
        std::shared_ptr<FrequencyInfo> freqInfo;
    };
}
