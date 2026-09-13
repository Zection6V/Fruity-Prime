#include "GainData.hpp"

namespace NCSFCommon::ReplayGain
{
    std::span<std::int32_t> GainData::Accum() noexcept
    {
        return accum;
    }

    double GainData::PeakSample() const noexcept
    {
        return peakSample;
    }

    void GainData::PeakSample(double value) noexcept
    {
        peakSample = value;
    }
}
