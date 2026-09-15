#pragma once

#include "GainData.hpp"

namespace NCSFCommon::ReplayGain
{
    class TrackGain;

    class AlbumGain
    {
    public:
        AlbumGain() = default;

        AlbumGain(const AlbumGain&) = delete;
        AlbumGain(AlbumGain&&) = delete;
        AlbumGain& operator=(const AlbumGain&) = delete;
        AlbumGain& operator=(AlbumGain&&) = delete;

        void AppendTrackData(TrackGain& trackGain);

        double GetGain();
        double GetPeak();

    private:
        GainData albumData;
    };
}
