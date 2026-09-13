#include "AlbumGain.hpp"

#include "ReplayGain.hpp"
#include "TrackGain.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace
{
    [[nodiscard]] double DoubleMax(double left, double right) noexcept
    {
        if (left != right)
        {
            if (!std::isnan(left))
                return right < left ? left : right;
            return left;
        }
        return std::signbit(right) ? left : right;
    }

    [[nodiscard]] std::int32_t AddUnchecked(std::int32_t left, std::int32_t right) noexcept
    {
        return std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(left) + std::bit_cast<std::uint32_t>(right));
    }
}

namespace NCSFCommon::ReplayGain
{
    void AlbumGain::AppendTrackData(TrackGain& trackGain)
    {
        auto sourceAccum = trackGain.GainData.Accum();
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(sourceAccum.size()); ++i)
        {
            auto destinationAccum = albumData.Accum();
            const auto index = static_cast<std::size_t>(i);
            destinationAccum[index] = AddUnchecked(destinationAccum[index], sourceAccum[index]);
        }
        albumData.PeakSample(DoubleMax(albumData.PeakSample(), trackGain.GainData.PeakSample()));
    }

    double AlbumGain::GetGain()
    {
        return ReplayGain::AnalyzeResult(albumData.Accum());
    }

    double AlbumGain::GetPeak() const noexcept
    {
        return albumData.PeakSample() / ReplayGain::MaxSampleValue;
    }
}
